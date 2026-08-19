#include "SkyChartExporter.hpp"
#include "SkyChartExporterDialog.hpp"
#include "SkySceneExtractor.hpp"
#include "SkyChartRenderer.hpp"
#include "PdfExporter.hpp"
#include "ImageExporter.hpp"
#include "SvgExporter.hpp"
#include "AtlasLayoutManager.hpp"

#include <QSvgGenerator>



#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "StelApp.hpp"
#include "GridLinesMgr.hpp" 
#include "StelModuleMgr.hpp" 
#include "StelModule.hpp"
#include "StelPluginInterface.hpp"
#include "StelIniParser.hpp"
#include "StelCore.hpp"
#include "StelMovementMgr.hpp"  
#include "StarMgr.hpp"


#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

// ==========================================================
// CONSTRUCTOR
// ==========================================================

SkyChartExporter::SkyChartExporter(QObject* parent)
    : StelModule()
{
    setObjectName("SkyChartExporter");  // <-- esta línea es la clave
}

// ==========================================================
// Init-dinit
// ==========================================================


void SkyChartExporter::init()
{
    Q_INIT_RESOURCE(SkyChartExporter);

    if (!configDialog)
        configDialog = new SkyChartExporterDialog();

    addAction("actionShow_SkyChartExporter",
              "Sky Chart Exporter", "SkyChartExporter",
              configDialog, "visible", "");

    StelGui* gui = dynamic_cast<StelGui*>(
        StelApp::getInstance().getGui());

    if (gui)
    {
        toolbarButton = new StelButton(nullptr,
            QPixmap(":/SkyChartExporter/sce_on.png"),
            QPixmap(":/SkyChartExporter/sce_off.png"),
            QPixmap(":/graphicGui/miscGlow32x32.png"),
            "actionShow_SkyChartExporter", false, "");

        gui->getButtonBar()->addButton(
            toolbarButton, "065-pluginsGroup");
    }
}

void SkyChartExporter::deinit()
{
    if (toolbarButton) { delete toolbarButton; toolbarButton = nullptr; }
    if (configDialog)  { configDialog->deleteLater(); configDialog = nullptr; }
    
    // Liberar cache de escena
    m_sceneCacheValid = false;
    m_cachedScene = SkyScene();
}

double SkyChartExporter::getCallOrder(StelModuleActionName a) const
{
    if (a == StelModule::ActionDraw) return 99.0;
    return 0.0;
}


// ==========================================================
// CACHEO ZOOM Y NAVEGACIÓN
// ==========================================================

bool SkyChartExporter::cacheMatches(const SkyChartExportOptions& options) const
{
    // 1. Verificación de formato y resolución del lienzo/papel
    if (m_cachedOptions.exportFormat != options.exportFormat)
        return false;

    if (m_cachedOptions.paperSize != options.paperSize)
        return false;

    if (m_cachedOptions.orientation != options.orientation)
        return false;

    if (m_cachedOptions.imageWidthPx != options.imageWidthPx)
        return false;

    if (m_cachedOptions.imageHeightPx != options.imageHeightPx)
        return false;

    if (m_cachedOptions.dpi != options.dpi)
        return false;

    // 2. Verificación de márgenes
    if (std::abs(m_cachedOptions.marginLeft - options.marginLeft) > 0.001)
        return false;

    if (std::abs(m_cachedOptions.marginRight - options.marginRight) > 0.001)
        return false;

    if (std::abs(m_cachedOptions.marginTop - options.marginTop) > 0.001)
        return false;

    if (std::abs(m_cachedOptions.marginBottom - options.marginBottom) > 0.001)
        return false;

    // 3. Verificación de modos de color, escala y magnitudes
    if (m_cachedOptions.colorMode != options.colorMode)
        return false;

    if (m_cachedOptions.grayscale != options.grayscale)
        return false;

    if (std::abs(m_cachedOptions.scale - options.scale) > 0.001)
        return false;

    if (std::abs(m_cachedOptions.limitingMagnitude - options.limitingMagnitude) > 0.001)
        return false;

    if (m_cachedOptions.showHorizon != options.showHorizon)
        return false;

    if (StarMgr::getDesignationUsage() != m_cachedDesignationUsage)
        return false;

    // 4. Verificación de tiempo y movimiento en Stellarium
    auto* core = StelApp::getInstance().getCore();
    if (!core)
        return false;

    if (std::abs(core->getJD() - m_cachedJD) > 0.001)
        return false;

    if (auto* movementMgr = GETSTELMODULE(StelMovementMgr))
    {
        const double currentFov = movementMgr->getCurrentFov();
        if (std::abs(currentFov - m_cachedFov) > 0.01)
            return false;
    }

    // 5. Verificación de visibilidad de capas de la escena celeste
    SkySceneExtractor extractor;
    SkySceneState current = extractor.captureCurrentState();
    const SkySceneState& cached = m_cachedScene.state;

    if (current.starsVisible                   != cached.starsVisible)                   return false;
    if (current.starLabelsVisible              != cached.starLabelsVisible)              return false;
    if (current.constellationLinesVisible       != cached.constellationLinesVisible)      return false;
    if (current.constellationLabelsVisible      != cached.constellationLabelsVisible)     return false;
    if (current.constellationBoundariesVisible  != cached.constellationBoundariesVisible) return false;
    if (current.nebulaeVisible                  != cached.nebulaeVisible)                 return false;
    if (current.deepSkyLabelsVisible            != cached.deepSkyLabelsVisible)           return false;
    if (current.planetsVisible                  != cached.planetsVisible)                 return false;
    if (current.planetLabelsVisible             != cached.planetLabelsVisible)            return false;
    if (current.equatorialGridVisible           != cached.equatorialGridVisible)          return false;
    if (current.horizontalGridVisible           != cached.horizontalGridVisible)          return false;
    if (current.equatorVisible                  != cached.equatorVisible)                 return false;
    if (current.eclipticVisible                 != cached.eclipticVisible)                return false;
    if (current.meridianVisible                 != cached.meridianVisible)                return false;
    if (current.horizonLineVisible              != cached.horizonLineVisible)             return false;
    if (current.milkyWayVisible                 != cached.milkyWayVisible)                return false;

    // 6. Verificación de la lista de Astros Específicos.
    // Agregar/quitar objetivos, o cambiar su magnitud, fuente, negrita o cursiva
    // debe invalidar el caché; si no, la escena extraída sigue usando reglas viejas.
    if (m_cachedOptions.specificTargets.size() != options.specificTargets.size())
        return false;

    for (int i = 0; i < options.specificTargets.size(); ++i)
    {
        const SpecificAstroRule& a = m_cachedOptions.specificTargets.at(i);
        const SpecificAstroRule& b = options.specificTargets.at(i);

        if (a.id != b.id)
            return false;

        if (std::abs(a.limitingMagnitude - b.limitingMagnitude) > 0.001)
            return false;

        if (a.font != b.font)
            return false;

        if (a.bold != b.bold || a.italic != b.italic)
            return false;
    }

    return true;
}


// ==========================================================
// FUNCIÓN PRINCIPAL
// ==========================================================

namespace {
// QSvgGenerator (QtSvg) no respeta painter.setClipRect()/setClipPath() al
// serializar el archivo: es una limitación conocida del motor de pintura de
// Qt para SVG, no un bug de SkyChartRenderer. El clip SÍ funciona para PDF y
// para el preview (dispositivos con paint engine completo); para el SVG hay
// que forzar el recorte después de escrito el archivo, insertando un
// <clipPath> + <g clip-path="..."> estándar alrededor de todo lo dibujado.
// Cualquier visor SVG conforme al estándar (navegadores, Inkscape, etc.)
// respeta esto sin depender del motor de Qt.
bool clipSvgFileToPageRect(const QString& filePath, const QRectF& pageRect)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Buscamos específicamente la etiqueta raíz <svg ...>, NO el primer '>'
    // del archivo: QSvgGenerator antepone una declaración <?xml ...?> (y a
    // veces un <!DOCTYPE svg ...>), que también terminan en '>'. Insertar ahí
    // deja el <defs>/<g> FUERA del elemento raíz -> XML con dos nodos de
    // primer nivel -> "Extra content at the end of the document".
    int svgStart = content.indexOf(QStringLiteral("<svg"));
    if (svgStart < 0) return false;
    int svgTagEnd = content.indexOf('>', svgStart);
    if (svgTagEnd < 0) return false;
    svgTagEnd += 1;

    const QString clipInjection = QStringLiteral(
        "<defs><clipPath id=\"scePageClip\"><rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\"/></clipPath></defs>"
        "<g clip-path=\"url(#scePageClip)\">")
        .arg(pageRect.x(), 0, 'f', 3)
        .arg(pageRect.y(), 0, 'f', 3)
        .arg(pageRect.width(), 0, 'f', 3)
        .arg(pageRect.height(), 0, 'f', 3);

    content.insert(svgTagEnd, clipInjection);

    int closeTagPos = content.lastIndexOf(QStringLiteral("</svg>"));
    if (closeTagPos < 0) return false;
    content.insert(closeTagPos, QStringLiteral("</g>"));

    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream stream(&out);
    stream << content;
    out.close();
    return true;
}
} // namespace

bool SkyChartExporter::paintChart(QPainter& painter, const SkyChartExportOptions& options,
                                   SkyScene* outScene, QRectF* outContentRect)
{     

            qDebug() << "[DEBUG-3 Exporter] paintChart recibido, specificTargets.size()="
             << options.specificTargets.size()
             << "| m_sceneCacheValid=" << m_sceneCacheValid
             << "| cacheMatches=" << (m_sceneCacheValid ? cacheMatches(options) : false);
    // ==========================================================
    // CACHEO DE ESCENA
    // Si la escena cacheada sigue siendo válida (mismo limitingMag
    // y mismo tiempo de Stellarium), la reutilizamos. Esto elimina
    // el cuello de botella al hacer zoom o re-renderizar.
    // ==========================================================
    SkyScene* scene = nullptr;

    if (m_sceneCacheValid && cacheMatches(options)) {
        scene = &m_cachedScene;
    } else {
        SkySceneExtractor extractor;
        if (!extractor.extractScene(m_cachedScene, options))
            return false;

       m_cachedOptions = options;
        auto* core = StelApp::getInstance().getCore();
        m_cachedDesignationUsage = StarMgr::getDesignationUsage();
        m_cachedJD = core ? core->getJD() : 0.0;
        if (auto* movementMgr = GETSTELMODULE(StelMovementMgr))   // NUEVO
            m_cachedFov = movementMgr->getCurrentFov();            // NUEVO
        m_sceneCacheValid = true;
        scene = &m_cachedScene;
    }

    AtlasLayoutManager layout;
    QRectF pageRect = layout.buildPageRect(options);
    QRectF contentRect = layout.getContentRect(options);

    // El PDF "se ve" recortado por accidente: QPdfWriter es un dispositivo de
    // página física de tamaño fijo, así que cualquier trazo dibujado fuera del
    // MediaBox simplemente no llega a existir en el archivo. QSvgGenerator, en
    // cambio, es un lienzo vectorial: cualquier elemento dibujado fuera del
    // viewBox se escribe igual en el <svg> y varios visores no aplican el
    // 'overflow: hidden' por defecto del elemento raíz, por lo que el recorte
    // nunca ocurre. Al fijar el clip explícitamente aquí (método compartido por
    // PDF, SVG y el preview) el comportamiento deja de depender del dispositivo
    // de salida y es idéntico en los tres casos.
    painter.setClipRect(pageRect);

    SkyChartRenderer renderer;
    renderer.render(painter, contentRect, *scene, options);
    layout.renderLayout(painter, *scene, options);

    if (outScene) *outScene = *scene;
    if (outContentRect) *outContentRect = contentRect;
    return true;
}


// ==========================================================
// EXPORTACION DEL PDF
// ==========================================================

bool SkyChartExporter::exportToPdf(const QString& filePath, const SkyChartExportOptions& options)
{
    if (filePath.isEmpty()) return false;

    QPdfWriter writer(filePath);
    // PdfExporter solo configura el dispositivo (tamaño, orientación, resolución, ruta)
    PdfExporter::configureWriter(writer, options); // método nuevo, de bajo nivel

    QPainter painter(&writer);
    if (!painter.isActive()) return false;

    bool ok = paintChart(painter, options);
    painter.end();
    return ok;
}

// ==========================================================
// EXPORTACION DE SVG
// ==========================================================
// Idéntico patrón a exportToPdf(): SvgExporter solo configura el
// dispositivo (QSvgGenerator), y paintChart() hace el resto exactamente
// igual que para el PDF (misma extracción/caché de escena, mismo
// renderer, mismo layout). Por eso NO se duplica aquí la lógica de
// caché que sí hizo falta duplicar en exportToImage() (ese caso es
// distinto porque ImageExporter necesita construir su propio QImage a
// una resolución en píxeles, no comparte el mismo QPaintDevice genérico).
// ==========================================================

bool SkyChartExporter::exportToSvg(const QString& filePath, const SkyChartExportOptions& options)
{
    if (filePath.isEmpty()) return false;

    QSvgGenerator generator;
    generator.setFileName(filePath);
    SvgExporter::configureGenerator(generator, options);

    QPainter painter(&generator);
    if (!painter.isActive()) return false;

    bool ok = paintChart(painter, options);
    painter.end();
    if (!ok) return false;

    AtlasLayoutManager layout;
    QRectF pageRect    = layout.buildPageRect(options);   // <- necesario primero, aunque no se use directo aquí
    QRectF contentRect = layout.getContentRect(options);  // ahora sí válido
    return clipSvgFileToPageRect(filePath, contentRect);
}

// ==========================================================
// EXPORTACION DE IMAGEN (JPEG/PNG)
// ==========================================================

bool SkyChartExporter::exportToImage(const QString& filePath, const SkyChartExportOptions& options)
{
    if (filePath.isEmpty()) return false;

    // ==========================================================
    // Misma lógica de caché de escena que usa paintChart() más arriba.
    // Se duplica aquí (en vez de reusar paintChart) porque ImageExporter
    // necesita construir su propio QImage a la resolución/DPI exacta,
    // y paintChart() está pensado para pintar sobre un QPainter ya
    // existente (el de QPdfWriter o el del preview).
    // ==========================================================
    SkyScene* scene = nullptr;

    if (m_sceneCacheValid && cacheMatches(options)) {
        scene = &m_cachedScene;
    } else {
        SkySceneExtractor extractor;
        if (!extractor.extractScene(m_cachedScene, options))
            return false;

        m_cachedOptions = options;
        auto* core = StelApp::getInstance().getCore();
        m_cachedDesignationUsage = StarMgr::getDesignationUsage();
        m_cachedJD = core ? core->getJD() : 0.0;
        if (auto* movementMgr = GETSTELMODULE(StelMovementMgr))
            m_cachedFov = movementMgr->getCurrentFov();
        m_sceneCacheValid = true;
        scene = &m_cachedScene;
    }

    AtlasLayoutManager layout;
    QRectF pageRect = layout.buildPageRect(options);
    QRectF contentRect = layout.getContentRect(options);

    ImageExporter imageExporter;
    return imageExporter.exportSceneWithOptions(filePath, *scene, options, pageRect, contentRect);
}

// ==========================================================
// DISPATCHER GENERICO: elige PDF o Imagen según options.exportFormat
// Pensado para que el Dialog (etapa de UI) llame a un único método,
// sin tener que preguntar el formato él mismo.
// ==========================================================

bool SkyChartExporter::exportChart(const QString& filePath, const SkyChartExportOptions& options)
{
    if (options.exportFormat.compare("PDF", Qt::CaseInsensitive) == 0)
        return exportToPdf(filePath, options);

    if (options.exportFormat.compare("SVG", Qt::CaseInsensitive) == 0)
        return exportToSvg(filePath, options);

    // Cualquier otro valor de exportFormat se trata como raster: JPEG o PNG
    return exportToImage(filePath, options);
}

// ==========================================================
// GENERAR PREVIEW
// ==========================================================

SkyScene SkyChartExporter::generarPreview(QImage& outImage, int w, int h,
                                           const SkyChartExportOptions& options)
{
    outImage = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    outImage.fill(Qt::white);
    QPainter painter(&outImage);

    AtlasLayoutManager layout;
    QRectF pageRect = layout.buildPageRect(options);

    const double scaleX = pageRect.width() > 0 ? static_cast<double>(w) / pageRect.width() : 1.0;
    const double scaleY = pageRect.height() > 0 ? static_cast<double>(h) / pageRect.height() : 1.0;
    const double scaleFactor = qMin(scaleX, scaleY);
    
    const double dx = (w - (pageRect.width() * scaleFactor)) / 2.0;
    const double dy = (h - (pageRect.height() * scaleFactor)) / 2.0;
    
    painter.translate(dx, dy);
    painter.scale(scaleFactor, scaleFactor);

    SkyScene scene;
    paintChart(painter, options, &scene);
    painter.end();
    return scene;
}
 //------------------------------------------------------
 // Interfaz
 //------------------------------------------------------


StelModule* SkyChartExporterStelPluginInterface::getStelModule() const
{
    return new SkyChartExporter();
}

StelPluginInfo SkyChartExporterStelPluginInterface::getPluginInfo() const
{
    StelPluginInfo info;
    info.id          = "SkyChartExporter";
    info.displayedName = "Sky Chart Exporter";
    info.authors     = "Asmat Vásquez";
    info.contact     = "asmatcentenofrancisco@gmail.com";
    info.description = "Exports sky charts to PDF.";
    info.version     = "26.1.0";
    return info;
}