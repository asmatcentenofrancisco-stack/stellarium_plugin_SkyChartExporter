#include "ImageExporter.hpp"
#include "AtlasLayoutManager.hpp"
#include "SkyChartRenderer.hpp"
#include "SkyScene.hpp"

#include <QPainter>
#include <QFileInfo>
#include <QtMath>
#include <algorithm>

// ==========================================================
// Calidad JPEG interna. NO es un control de usuario ("peso"
// fue descartado); es simplemente el valor que Qt exige para
// poder guardar un archivo con compresión con pérdida.
// ==========================================================
namespace {
    constexpr int kDefaultJpegQuality = 92;

    bool isJpeg(const QString& format)
    {
        return format.compare("JPEG", Qt::CaseInsensitive) == 0
            || format.compare("JPG",  Qt::CaseInsensitive) == 0;
    }
}

// ==========================================================
// CONSTRUCTOR
// ==========================================================

ImageExporter::ImageExporter()
    : m_path(""), m_dpi(300)
{
}

void ImageExporter::setOutputPath(const QString& path) { m_path = path; }
void ImageExporter::setResolution(int dpi)             { m_dpi = dpi; }

// ==========================================================
// API SIMPLE
// ==========================================================

bool ImageExporter::exportScene(const SkyScene& scene)
{
    if (m_path.isEmpty())
        return false;

    // El formato se infiere de la extensión del archivo (JPG/JPEG/PNG/TIFF).
    const QString suffix = QFileInfo(m_path).suffix().toUpper();

    SkyChartExportOptions defaults;
    defaults.exportFormat = suffix.isEmpty() ? QStringLiteral("PNG") : suffix;
    defaults.dpi          = m_dpi;

    QImage image = createTargetImage(defaults);
    if (image.isNull())
        return false;

    QPainter painter(&image);
    if (!painter.isActive())
        return false;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRectF pageRect(0, 0, image.width(), image.height());

    SkyChartRenderer renderer;
    renderer.render(painter, pageRect, scene, defaults);

    painter.end();

    return image.save(m_path,
                       defaults.exportFormat.toUtf8().constData(),
                       isJpeg(defaults.exportFormat) ? kDefaultJpegQuality : -1);
}

// ==========================================================
// API AVANZADA (la que usa el orquestador SkyChartExporter)
// ==========================================================

bool ImageExporter::exportSceneWithOptions(const QString& filePath,
                                            const SkyScene& scene,
                                            const SkyChartExportOptions& options,
                                            const QRectF& pageRect,
                                            const QRectF& contentRect)
{
    QImage image = createTargetImage(options);
    if (image.isNull())
        return false;

    // ------------------------------------------------------------------
    // FIX: doble escalado de texto.
    //
    // createTargetImage() deja la imagen con el DPI REAL (options.dpi) como
    // metadata (dotsPerMeterX/Y). Qt usa esa metadata para convertir los
    // QFont (definidos en puntos) a píxeles al pintar sobre esta QImage.
    // Es decir: un QFont de 9pt ya se dibuja escalado al DPI real ANTES de
    // que este método toque nada.
    //
    // Pero más abajo aplicamos painter.scale(scaleFactor) para llevar las
    // coordenadas "de puntos" (72 pt/pulgada) que usan AtlasLayoutManager y
    // SkyChartRenderer -- el mismo sistema que usa PdfExporter, que trabaja
    // a resolution=72, o sea 1 punto = 1 unidad -- a los píxeles reales de
    // la imagen. Esa escala es necesaria para la geometría pura (líneas,
    // círculos, radios de estrella), que no tiene DPI propio.
    //
    // El texto termina escalado DOS veces: una por la metadata DPI de la
    // imagen, y otra por painter.scale(). Las estrellas y la cuadrícula
    // sólo se escalan una vez (por eso solo el texto sale desproporcionado).
    //
    // Solución: durante el pintado forzamos el DPI de la imagen a 72 (para
    // que 1 punto = 1 píxel, igual que en el PDF) y dejamos que
    // painter.scale() sea la ÚNICA escala, tanto para texto como para
    // geometría. Recién al final, antes de guardar, restauramos el DPI
    // real como metadata del archivo -- eso no reescala los píxeles ya
    // pintados, solo corrige la resolución física reportada para impresión.
    const int realDpi = (options.dpi > 0) ? options.dpi : 300;
    image.setDotsPerMeterX(qRound(72.0 / 0.0254));
    image.setDotsPerMeterY(qRound(72.0 / 0.0254));
    // ------------------------------------------------------------------

    QPainter painter(&image);
    if (!painter.isActive())
        return false;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // AtlasLayoutManager::buildPageRect() trabaja en "puntos" (72 pt/pulgada).
    // 'pageRect' (en puntos) es el mismo que usa el PDF/SVG; escalamos el
    // painter para que ese sistema de coordenadas mapee exactamente a los
    // píxeles reales de esta imagen -- ya sea que el tamaño salga del cálculo
    // por DPI o de un override explícito en imageWidthPx/imageHeightPx. Así
    // el renderer y el layout dibujan igual para los tres motores; solo
    // cambia la escala del painter.
    //
    // FIX: este método YA RECIBE 'pageRect' y 'contentRect' calculados una
    // sola vez por el orquestador (SkyChartExporter::exportToImage()), que
    // es la fuente única de verdad -- exactamente el mismo valor que usa
    // PdfExporter. Antes, este bloque los ignoraba por completo y creaba su
    // propia instancia de AtlasLayoutManager para recalcularlos desde
    // 'options', desincronizado del orquestador. Con las options actuales
    // ambos cálculos coinciden, pero en cuanto el orquestador reajuste el
    // pageRect/contentRect para un nuevo formato (recorte, aspecto custom,
    // etc.), ImageExporter seguía usando su cálculo propio y desactualizado
    // -> el lienzo no se reajustaba y el contenido quedaba cortado.
    //
    // Seguimos necesitando 'layout.buildPageRect(options)' aquí (recalcula
    // el mismo resultado, pero como efecto secundario fija el estado interno
    // m_pageRect/m_contentRect que layout.renderLayout() usa más abajo para
    // dibujar marco, header, footer y leyendas -- igual que hace
    // PdfExporter). Lo que cambia es que el ESCALADO y el RENDER DEL CIELO
    // ahora usan los parámetros recibidos, no el recálculo local.
    AtlasLayoutManager layout;
    layout.buildPageRect(options); // necesario para fijar m_contentRect/m_pageRect internos

    const double scaleX = pageRect.width() > 0 ? static_cast<double>(image.width()) / pageRect.width() : 1.0;
    const double scaleY = pageRect.height() > 0 ? static_cast<double>(image.height()) / pageRect.height() : 1.0;
    const double scaleFactor = qMin(scaleX, scaleY);
    
    const double dx = (image.width() - (pageRect.width() * scaleFactor)) / 2.0;
    const double dy = (image.height() - (pageRect.height() * scaleFactor)) / 2.0;
    
    painter.translate(dx, dy);
    painter.scale(scaleFactor, scaleFactor);

    SkyChartRenderer renderer;
    renderer.render(painter, contentRect, scene, options); // 1º: fondo + cielo

    layout.renderLayout(painter, scene, options);            // 2º: marco, título, leyendas, norte ENCIMA

    painter.end();

    // Restaurar el DPI real en la metadata del archivo (solo afecta la
    // resolución física reportada, no los píxeles ya pintados).
    image.setDotsPerMeterX(qRound(realDpi / 0.0254));
    image.setDotsPerMeterY(qRound(realDpi / 0.0254));

    return image.save(filePath,
                       options.exportFormat.toUtf8().constData(),
                       isJpeg(options.exportFormat) ? options.imageQuality : -1);
}

// ==========================================================
// CREACIÓN DEL LIENZO DESTINO
// ==========================================================

QImage ImageExporter::createTargetImage(const SkyChartExportOptions& options)
{
    // Si el usuario fijó resolución exacta en píxeles, se respeta tal cual
    // y se ignora el cálculo por papel + DPI.
    if (options.imageWidthPx > 0 && options.imageHeightPx > 0)
    {
        QImage image(options.imageWidthPx, options.imageHeightPx, QImage::Format_ARGB32_Premultiplied);
        const int dpi = (options.dpi > 0) ? options.dpi : 300;
        image.setDotsPerMeterX(qRound(dpi / 0.0254));
        image.setDotsPerMeterY(qRound(dpi / 0.0254));
        image.fill(Qt::white);
        return image;
    }

    // Misma tabla de tamaños de papel que AtlasLayoutManager::buildPageRect(),
    // convertida a píxeles según DPI en vez de a puntos.
    constexpr double mmToInch = 1.0 / 25.4;

    double widthMm = 210.0, heightMm = 297.0; // A4 por defecto

    if (options.paperSize == "A3")         { widthMm = 297.0; heightMm = 420.0; }
    else if (options.paperSize == "A2")    { widthMm = 420.0; heightMm = 594.0; }
    else if (options.paperSize == "Carta") { widthMm = 215.9; heightMm = 279.4; }
    else if (options.paperSize == "Legal") { widthMm = 215.9; heightMm = 355.6; }

    if (options.orientation == "Horizontal")
        std::swap(widthMm, heightMm);

    const int dpi = (options.dpi > 0) ? options.dpi : 300;

    const int pxWidth  = qRound(widthMm  * mmToInch * dpi);
    const int pxHeight = qRound(heightMm * mmToInch * dpi);

    QImage image(pxWidth, pxHeight, QImage::Format_ARGB32_Premultiplied);
    image.setDotsPerMeterX(qRound(dpi / 0.0254));
    image.setDotsPerMeterY(qRound(dpi / 0.0254));
    image.fill(Qt::white);
    return image;
}