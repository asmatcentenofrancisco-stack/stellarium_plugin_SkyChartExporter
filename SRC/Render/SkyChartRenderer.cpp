#include "SkyChartRenderer.hpp"
#include "SkyChartExporterOptions.hpp"
#include <QtMath>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QPainterPath>
#include <limits>

SkyChartRenderer::SkyChartRenderer()
{
    m_scale = 1.0;
}


bool SkyChartRenderer::render(QPainter& painter, const QRectF& pageRect,
                              const SkyScene& scene, const SkyChartExportOptions& options)
{
    if (!painter.isActive())
        return false;

   // IMPORTANTE: `pageRect` que llega aquí YA es el contentRect (la página
    // menos los márgenes que puso el usuario en el diálogo).
    //
    // Margen de seguridad cartográfica del 3% para evitar cortes en etiquetas
    // de los bordes (como el lado izquierdo). Este `sceneRect` (reducido) es
    // el que usan estrellas, grid/polilíneas, nebulosas y sistema solar, y
    // es DELIBERADO que sea el mismo para los cuatro: el algoritmo de
    // anclaje de etiquetas de grado/hora en drawPolylines() (ver
    // findLabelFrameCrossing) depende de que la geometría del grid quede
    // más adentro que el labelFrame que usa para probar cruces de borde —
    // si esa relación de tamaños cambia, el anclaje de etiquetas empieza a
    // usar otro camino del algoritmo y las etiquetas de horas/grados se
    // mezclan entre sí (bug ya visto: no tocar este rect salvo que también
    // se revise findLabelFrameCrossing).
    //
    // La ÚNICA excepción es la Vía Láctea: al ser una mancha de fondo
    // difusa (no un catálogo de puntos que deba registrar con el grid con
    // precisión), se dibuja sobre `milkyWayRect`, SIN el recorte del 3%,
    // para que llegue hasta el margen real de la página en vez de cortarse
    // antes de tiempo.
    QRectF paddedPageRect = pageRect.adjusted(pageRect.width() * 0.03, pageRect.height() * 0.03, 
                                              -pageRect.width() * 0.03, -pageRect.height() * 0.03);

    // Se ajusta SIEMPRE por el alto de la captura de Stellarium, sin
    // importar la orientación de la hoja (vertical u horizontal). Esto
    // preserva la proporción real del cielo capturado (sin deformarlo) y
    // evita generar información falsa (coordenadas estiradas o rellenadas).
    // El ancho que no entra en la hoja simplemente se recorta con el clip
    // de más abajo (painter.setClipRect(pageRect)) — no se dibuja de más
    // ni se comprime, solo se corta lo que sobra.
    double sourceAspect = 1.0;
    if (scene.viewportSize.height() > 0.0)
        sourceAspect = scene.viewportSize.width() / scene.viewportSize.height();

    const double drawHeight = paddedPageRect.height();
    const double drawWidth  = drawHeight * sourceAspect;

    QRectF sceneRect(0, 0, drawWidth, drawHeight);
    sceneRect.moveCenter(paddedPageRect.center());

    // Mismo cálculo que sceneRect, pero partiendo del pageRect COMPLETO (sin
    // el 3%): solo para la Vía Láctea.
    const double mwDrawHeight = pageRect.height();
    const double mwDrawWidth  = mwDrawHeight * sourceAspect;
    QRectF milkyWayRect(0, 0, mwDrawWidth, mwDrawHeight);
    milkyWayRect.moveCenter(pageRect.center());

    m_pageRect = sceneRect;
    m_scale = options.scale;

    painter.save();
    setupPainter(painter);
    painter.setClipRect(pageRect); // el recorte real: todo lo que sale del contentRect se corta aquí

    drawBackground(painter, pageRect, scene);  // fondo blanco cubre TODO el contentRect, sin huecos
    drawGrid(painter, sceneRect, scene);
    drawMilkyWay(painter, milkyWayRect, scene, options);
    drawNebulae(painter, sceneRect, scene);
    drawStars(painter, sceneRect, scene, options);
    drawSolarSystem(painter, sceneRect, scene);
    drawPolylines(painter, sceneRect, pageRect, scene);
    drawPolygons(painter, sceneRect, scene);
    drawTexts(painter, sceneRect, scene);
    drawPolylines(painter, sceneRect, pageRect, scene); // pageRect es el parámetro de render(), NO sceneRect

    painter.restore();
    return true;
}

void SkyChartRenderer::setupPainter(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

QPointF SkyChartRenderer::projectPoint(const QPointF& normalizedPoint, const QRectF& pageRect) const
{
    const double x = pageRect.left() + (normalizedPoint.x() + 1.0) * 0.5 * pageRect.width();
    const double y = pageRect.top() + (1.0 - (normalizedPoint.y() + 1.0) * 0.5) * pageRect.height();
    return QPointF(x, y);
}

double SkyChartRenderer::scaleRadius(double radius) const
{
    return radius * m_scale;
}

void SkyChartRenderer::drawBackground(QPainter& painter, const QRectF& pageRect, const SkyScene& scene)
{
    Q_UNUSED(scene);
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white); // Fondo blanco para impresión y vista previa
    painter.drawRect(pageRect);
    painter.restore();
}

void SkyChartRenderer::drawGrid(QPainter& painter, const QRectF& pageRect, const SkyScene& scene)
{
    // La cuadrícula ya no se hardcodea aquí: SkySceneExtractor::captureGrid()
    // genera la geometría real (ecuatorial u horizontal) como SkyPolyline
    // dentro de scene.polylines, y drawPolylines() la dibuja más abajo en
    // el mismo render(). Esta función queda intencionalmente vacía.
    Q_UNUSED(painter);
    Q_UNUSED(pageRect);
    Q_UNUSED(scene);
}


double SkyChartRenderer::starRadiusForMagnitude(double magnitude, double limitingMagnitude, double scale)
{
    const double magDiff = qMax(0.02, limitingMagnitude - magnitude);
    const double area = std::pow(magDiff, 1.15);
    return qBound(0.18, 0.85 * std::sqrt(area), 5.5) * scale;
}


double SkyChartRenderer::bodyRadiusForMagnitude(double magnitude, double limitingMagnitude, double scale)
{
    // Umbral: la estrella más brillante del cielo real (Sirio, mag ~ -1.5).
    // Por debajo de este límite (más brillante que -1.5), un objeto ya no
    // es "una estrella más" — es territorio exclusivo de Sol, Luna, Venus,
    // Júpiter. La curva de estrellas (compresiva, pensada para miles de
    // puntos) se queda demasiado plana ahí, por eso Marte/Venus/la Luna
    // terminaban con el mismo tamaño que cualquier estrella brillante.
    constexpr double brightestStarMag = -1.5;

    if (magnitude >= brightestStarMag)
    {
        // Dentro del rango estelar normal: mismo comportamiento que las
        // estrellas, para que Marte (mag ~0.9 en tu log) se vea coherente
        // con una estrella de brillo similar, como ya confirmaste que
        // ocurre correctamente en Cartes du Ciel.
        return starRadiusForMagnitude(magnitude, limitingMagnitude, scale);
    }

    // Más brillante que Sirio: crecimiento adicional, menos comprimido,
    // proporcional a cuántas magnitudes de más brillo tiene sobre ese piso.
    const double baseRadius = starRadiusForMagnitude(brightestStarMag, limitingMagnitude, scale);
    const double extraMagnitudes = brightestStarMag - magnitude; // > 0
    const double growth = 1.0 + extraMagnitudes * 0.35; // coeficiente ajustable

    // Techo amplio (no el 5.5 de las estrellas) para permitir que el Sol,
    // el objeto más brillante posible, pueda dominar visualmente el plano
    // sin límite artificial de campo estelar.
    return qMin(baseRadius * growth, 40.0);
}

bool SkyChartRenderer::starIsFilled(double magnitude)
{
    // Brillantes: círculo relleno. Tenues: círculo hueco (casi un punto),
    // igual que en los atlas impresos clásicos.
    return magnitude < 3.5;
}

void SkyChartRenderer::drawStars(QPainter& painter, const QRectF& pageRect,
                                  const SkyScene& scene, const SkyChartExportOptions& options)
{
    painter.save();
    struct PendingLabel { QPointF pos; double r; QString text; QFont font; };
    QVector<PendingLabel> labels;

    for (const SkyPoint& star : scene.stars)
    {
        // ¿Esta estrella tiene una regla de la sección Específica asociada?
        const SpecificAstroRule* specific = nullptr;

        for (const auto& target : scene.specificTargets)
        {
            if (target.id == star.id || star.id.startsWith(target.id + " ")) { specific = &target; break; }
        }

        if (!scene.specificTargets.isEmpty())
            qDebug() << "[DEBUG-6 Renderer] star.id=" << star.id
                     << (specific ? "-> MATCH, font=" + specific->font.family() : "-> SIN MATCH");

        // VISIBILIDAD: una estrella específica usa SU PROPIA magnitud límite,
        // no la General. El extractor ya la incluyó en scene.stars bajo ese
        // criterio; aquí NO hay que volver a filtrarla con options.limitingMagnitude,
        // o se pierde (esto es lo que hacía desaparecer a las estrellas tenues).
        const double visibilityLimit = specific ? specific->limitingMagnitude : options.limitingMagnitude;
        if (star.magnitude > visibilityLimit) continue;

        if (star.position.x() < -1.0 || star.position.x() > 1.0 ||
            star.position.y() < -1.0 || star.position.y() > 1.0) continue;

        QPointF pos = projectPoint(star.position, pageRect);
        QColor color = star.pen.color();
        double r = starRadiusForMagnitude(star.magnitude, options.limitingMagnitude, m_scale);

        painter.setPen(Qt::NoPen);
        painter.setBrush(color.isValid() ? color : Qt::black);
        painter.drawEllipse(pos, r, r);

        // ETIQUETA: una estrella específica SIEMPRE muestra su etiqueta (para eso
        // se marcó como específica/destacada), sin depender del umbral General
        // de etiquetas. Las demás siguen la regla General de siempre.
        const bool showLabel = specific ? true : (star.magnitude <= options.starLabelMagnitude);

        if (showLabel && !star.label.isEmpty())
        {
            QFont labelFont = specific ? specific->font : options.starLabelFont;
            labels.push_back({pos, r, star.label, labelFont});
        }
    }

    // segunda pasada: todas las etiquetas por encima de todos los círculos,
    // cada una con su propia fuente (general o específica según corresponda).
    painter.setPen(Qt::black);
    for (const auto& l : labels)
    {
        painter.setFont(l.font);
        painter.drawText(l.pos + QPointF(l.r + 2, 3), l.text);
    }

    painter.restore();
}

void SkyChartRenderer::drawSolarSystem(QPainter& painter, const QRectF& pageRect, const SkyScene& scene)
{
    painter.save();
    for (const SkySymbol& p : scene.planets)
    {
        QPointF pos = projectPoint(p.position, pageRect);
        
      const double pxScale = pageRect.width() / scene.viewportSize.width();
// El factor de escala de usuario (Escala) ya viene incluido en p.scale
// desde el extractor (userDiscScale). No se vuelve a multiplicar por
// m_scale aquí -> antes esto producía escala^2 para planetas/Sol/Luna
// frente a escala^1 para las estrellas, causando el crecimiento
// desproporcionado al subir el slider.
const double r = qMax(1.0, p.scale * pxScale);

       if (p.name == "Luna" || p.name == "Moon")
{
    double limbAngleDeg = 0.0;
    if (p.hasSunDirection)
    {
        QPointF sunPagePos = projectPoint(p.sunDirection, pageRect);
        QPointF dir = sunPagePos - pos;
        if (dir.x() != 0.0 || dir.y() != 0.0)
            limbAngleDeg = std::atan2(dir.y(), dir.x()) * 180.0 / M_PI;
    }
    drawMoonPhase(painter, pos, r, p.rotation, limbAngleDeg, p.color);
}

        else
        {
            // Estilo sólido con contorno sutil para el Sol y los planetas
            painter.setPen(QPen(p.color.darker(150), 0.8));
            painter.setBrush(p.color);
            painter.drawEllipse(pos, r, r);
        }
    }
    painter.restore();
}


void SkyChartRenderer::drawMoonPhase(QPainter& painter, const QPointF& pos, double r,
                                      double illuminatedFraction, double limbAngleDeg,
                                      const QColor& color)
{
    painter.save();
    painter.translate(pos);
    painter.rotate(limbAngleDeg);   // orienta el eje "+X local" hacia el Sol real

    const double k = qBound(0.0, illuminatedFraction, 1.0);

    painter.setPen(QPen(color, 1.0));
    painter.setBrush(QColor(25, 25, 25));
    painter.drawEllipse(QPointF(0,0), r, r);

    if (k > 0.01) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);

        if (k >= 0.99) {
            painter.drawEllipse(QPointF(0,0), r, r);
        } else {
            QPainterPath path;
            double waist = (0.5 - k) * 2.0 * r;  // ver nota abajo

            path.moveTo(0, -r);
            path.arcTo(QRectF(-r, -r, 2*r, 2*r), 90, -180);
            path.cubicTo(QPointF(waist, r*0.55), QPointF(waist, -r*0.55), QPointF(0, -r));
            path.closeSubpath();
            painter.drawPath(path);
        }
    }
    painter.restore();
}

void SkyChartRenderer::drawNebulae(QPainter& painter, const QRectF& pageRect, const SkyScene& scene)
{
    painter.save();
    painter.setBrush(Qt::NoBrush);

    const double pxScale = pageRect.width() / scene.viewportSize.width();

    // Detección del modo de color para aplicar a la paleta base
    bool isMonocromo = false;
    bool isGrayscale = scene.state.grayscale;
    if (!scene.nebulae.isEmpty()) {
        if (scene.nebulae.first().color == Qt::black) isMonocromo = true;
    }

    auto applyMode = [&](QColor c) -> QColor {
        if (isMonocromo) return Qt::black;
        if (isGrayscale) { int g = qGray(c.rgb()); return QColor(g, g, g); }
        return c;
    };

    const QColor colorGalaxia = applyMode(QColor(220, 50, 50));      // Rojo
    const QColor colorCumulo  = applyMode(QColor(240, 200, 50));      // Amarillo / Dorado
    const QColor colorNebulosa= applyMode(QColor(50, 190, 50));       // Verde
    const QColor colorOscura  = applyMode(QColor(120, 120, 120));     // Gris neutro

    for (const SkySymbol& n : scene.nebulae)
    {
        QPointF pos = projectPoint(n.position, pageRect);
        
        // Eje mayor y eje menor
        const double r = qMax(1.0, n.scale * pxScale);
        const double ry = qMax(1.0, (n.scaleY > 0.0 ? n.scaleY : n.scale) * pxScale);

        auto attenuated = [&](QColor baseColor, int baseAlpha, double sizeSensitivity) {
            const double maxDimension = qMax(r, ry);
            const double sizePenalty = qBound(0.0, (maxDimension - 4.0) / 22.0, 1.0) * sizeSensitivity;
            QColor c = baseColor;
            c.setAlpha(qBound(15, static_cast<int>(baseAlpha * (1.0 - sizePenalty)), 255));
            return c;
        };

        switch (n.type)
        {
        case SkySymbolType::Galaxy:
        {
            painter.setPen(QPen(attenuated(colorGalaxia, 200, 0.35), 0.9, Qt::SolidLine));
            painter.save();
            painter.translate(pos);
            painter.rotate(n.rotation);
            double drawRy = (n.scaleY > 0 && n.scaleY != n.scale) ? ry : (r * 0.55);
            painter.drawEllipse(QPointF(0, 0), r, drawRy);
            painter.restore();
            break;
        }
        case SkySymbolType::OpenCluster:
        {
            painter.setPen(QPen(colorCumulo, 0.8, Qt::DotLine));
            painter.drawEllipse(pos, r, r);
            break;
        }
        case SkySymbolType::GlobularCluster:
        {
            const QColor c = attenuated(colorCumulo, 170, 0.75);
            painter.setPen(QPen(c, 0.8, Qt::SolidLine));
            painter.drawEllipse(pos, r, r);
            painter.drawLine(QPointF(pos.x() - r, pos.y()), QPointF(pos.x() + r, pos.y()));
            painter.drawLine(QPointF(pos.x(), pos.y() - r), QPointF(pos.x(), pos.y() + r));
            break;
        }
        case SkySymbolType::PlanetaryNebula:
        {
            const QColor c = attenuated(colorNebulosa, 200, 0.3);
            painter.setPen(QPen(c, 0.8, Qt::SolidLine));
            painter.drawEllipse(pos, r, r);
            painter.setBrush(c);
            painter.drawEllipse(pos, qMax(0.6, r * 0.15), qMax(0.6, r * 0.15));
            painter.setBrush(Qt::NoBrush);
            break;
        }
        case SkySymbolType::DarkNebula:
        {
            painter.setPen(QPen(attenuated(colorOscura, 110, 0.3), 0.7, Qt::DashLine));
            painter.save();
            painter.translate(pos);
            painter.rotate(n.rotation);
            painter.drawEllipse(QPointF(0, 0), r, ry);
            painter.restore();
            break;
        }
        case SkySymbolType::SkyRegion:
        {
            painter.setPen(QPen(attenuated(colorNebulosa, 160, 0.2), 0.8, Qt::DashLine));
            painter.save();
            painter.translate(pos);
            painter.rotate(n.rotation);
            painter.drawEllipse(QPointF(0, 0), r, ry);
            painter.restore();
            break;
        }
        case SkySymbolType::EmissionNebula:
        case SkySymbolType::DiffuseNebula:
        default:
        {
            const QColor c = attenuated(colorNebulosa, 190, 0.3);
            painter.setPen(QPen(c, 0.9, Qt::DashLine));
            painter.save();
            painter.translate(pos);
            painter.rotate(n.rotation);
            if (r != ry) {
                painter.drawEllipse(QPointF(0, 0), r, ry); 
            } else {
                painter.drawRoundedRect(QRectF(-r, -r, r * 2.0, r * 2.0), r * 0.6, r * 0.6); 
            }
            painter.restore();
            break;
        }
        }
    }
    painter.restore();
}


namespace {

// Busca si la polilínea cruza un borde HORIZONTAL fijo (edgeY), dentro del
// rango X del frame. Devuelve el punto de cruce en outPos.
bool crossesHorizontalEdge(const QVector<QPointF>& pts, const QRectF& frame,
                            double edgeY, QPointF& outPos)
{
    for (int i = 0; i + 1 < pts.size(); ++i)
    {
        const QPointF& a = pts[i];
        const QPointF& b = pts[i + 1];
        if ((a.y() - edgeY) * (b.y() - edgeY) <= 0.0 && a.y() != b.y())
        {
            const double t = (edgeY - a.y()) / (b.y() - a.y());
            const double x = a.x() + t * (b.x() - a.x());
            if (x >= frame.left() && x <= frame.right())
            {
                outPos = QPointF(x, edgeY);
                return true;
            }
        }
    }
    return false;
}

// Idéntico pero para un borde VERTICAL fijo (edgeX).
bool crossesVerticalEdge(const QVector<QPointF>& pts, const QRectF& frame,
                          double edgeX, QPointF& outPos)
{
    for (int i = 0; i + 1 < pts.size(); ++i)
    {
        const QPointF& a = pts[i];
        const QPointF& b = pts[i + 1];
        if ((a.x() - edgeX) * (b.x() - edgeX) <= 0.0 && a.x() != b.x())
        {
            const double t = (edgeX - a.x()) / (b.x() - a.x());
            const double y = a.y() + t * (b.y() - a.y());
            if (y >= frame.top() && y <= frame.bottom())
            {
                outPos = QPointF(edgeX, y);
                return true;
            }
        }
    }
    return false;
}

// FIX etiquetado ecuatorial fuera del polo:
// Antes esta función solo probaba UN par de bordes según el tipo de línea
// (meridianos -> solo borde superior; paralelos -> solo laterales). Esa
// convención asume una vista centrada en el polo, donde los meridianos
// siempre irradian hacia arriba y los paralelos siempre envuelven los
// laterales -- por eso el rótulo ecuatorial "funciona perfecto en los
// polos". Pero en una vista ecuatorial alejada del polo (mirando al
// Este/Oeste, FOV ancho), la proyección curva las líneas lo suficiente
// como para que muchos meridianos salgan del cuadro por un lateral (no por
// arriba) y algunos paralelos salgan por arriba/abajo (no por un lateral).
// Como antes solo se probaba el borde "natural", esas líneas nunca
// encontraban cruce y caían al fallback de punto más cercano (impreciso,
// ver más abajo) -> etiquetas ausentes o fuera de cuadro.
//
// Ahora se prueban los 4 bordes, dando prioridad al par natural del tipo
// de línea (se ve más prolijo cuando sí aplica, típico de la vista polar)
// pero cayendo a los otros dos si no hay cruce ahí.
bool findLabelFrameCrossing(const QVector<QPointF>& pts, const QRectF& frame,
                             bool isMeridianLine, QPointF& outPos)
{
    if (pts.isEmpty()) return false;

    // Aritmética pura: evaluamos la envergadura predominante de la curva proyectada
    const double dx = qAbs(pts.first().x() - pts.last().x());
    const double dy = qAbs(pts.first().y() - pts.last().y());

    if (isMeridianLine)
    {
        // Un meridiano siempre tiene el derecho natural a cruzar por arriba o abajo
        if (crossesHorizontalEdge(pts, frame, frame.top(),    outPos)) return true;
        if (crossesHorizontalEdge(pts, frame, frame.bottom(), outPos)) return true;
        
        // Exclusión matemática: Solo cae a los laterales si es matemáticamente 
        // más horizontal que vertical (Ej: meridianos radiando hacia los lados en una vista polar).
        if (dx > dy) 
        {
            if (crossesVerticalEdge(pts, frame, frame.left(),     outPos)) return true;
            if (crossesVerticalEdge(pts, frame, frame.right(),    outPos)) return true;
        }
    }
    else
    {
        // Un paralelo (declinación/altura) siempre tiene derecho a cruzar por los laterales
        if (crossesVerticalEdge(pts, frame, frame.left(),     outPos)) return true;
        if (crossesVerticalEdge(pts, frame, frame.right(),    outPos)) return true;
        
        // Exclusión matemática: Solo cae a los bordes superior/inferior si es 
        // más vertical que horizontal (círculos concéntricos en vistas polares).
        if (dy > dx) 
        {
            if (crossesHorizontalEdge(pts, frame, frame.top(),    outPos)) return true;
            if (crossesHorizontalEdge(pts, frame, frame.bottom(), outPos)) return true;
        }
    }
    return false;
}
} // namespace

void SkyChartRenderer::drawPolylines(QPainter& painter, const QRectF& sceneRect,
                                      const QRectF& clipRect, const SkyScene& scene)
{
    painter.save();

    struct PendingGridLabel
    {
        QPointF pos;
        QString text;
        QColor  color;
        double  angleDeg = 0.0;
        bool    drawBox = true;
        bool    growsNegative = true;
    };
    QVector<PendingGridLabel> gridLabels;

    const double labelMargin = 12.0;
    const QRectF labelFrame = clipRect.adjusted(labelMargin, labelMargin, -labelMargin, -labelMargin);

    for (const SkyPolyline& line : scene.polylines)
    {
        if (line.points.size() < 2) continue;

        QVector<QPointF> devicePts;
        devicePts.reserve(line.points.size());
        for (const QPointF& p : line.points)
            devicePts.append(projectPoint(p, sceneRect));

        QPen pen(line.color, line.width, line.style);
        painter.setPen(pen);
        QPainterPath path;
        path.moveTo(devicePts.first());
        for (int i = 1; i < devicePts.size(); ++i)
            path.lineTo(devicePts[i]);
        painter.drawPath(path);

        if (line.labelText.isEmpty()) continue;
        if (line.type != SkyPolylineType::GridLine && line.type != SkyPolylineType::EclipticLine) continue;

        QPointF anchor;
        bool foundAnchor = findLabelFrameCrossing(devicePts, labelFrame, line.isMeridianLine, anchor);

        if (!foundAnchor && !line.truncatedByHorizon)
        {
            // FIX: antes esto medía distancia en UN SOLO EJE (Y para
            // meridianos, X para paralelos), ignorando la otra coordenada.
            // Eso podía elegir como "más cercano" un punto cuya otra
            // coordenada cayera muy afuera del labelFrame -> la etiqueta
            // terminaba dibujada fuera del área visible (invisible o
            // recortada), justo lo que se veía como "faltan etiquetas" en
            // vistas ecuatoriales lejos del polo.
            //
            // Ahora se mide la distancia real al RECTÁNGULO del frame (0 si
            // el punto ya está adentro), y al final se hace clamp del ancla
            // para que, sin importar dónde cayó el punto más cercano de la
            // polilínea, la etiqueta siempre quede dentro del labelFrame.
            double best = std::numeric_limits<double>::max();
            for (const QPointF& p : devicePts)
            {
                const double dx = qMax(0.0, qMax(labelFrame.left() - p.x(), p.x() - labelFrame.right()));
                const double dy = qMax(0.0, qMax(labelFrame.top()  - p.y(), p.y() - labelFrame.bottom()));
                const double d = dx * dx + dy * dy;
                if (d < best) { best = d; anchor = p; foundAnchor = true; }
            }

            if (foundAnchor)
            {
                anchor.setX(qBound(labelFrame.left(), anchor.x(), labelFrame.right()));
                anchor.setY(qBound(labelFrame.top(),  anchor.y(), labelFrame.bottom()));
            }
        }

        if (!foundAnchor) continue;

        if (line.type == SkyPolylineType::GridLine)
        {
            PendingGridLabel gl;
            gl.pos   = anchor;
            gl.text  = line.labelText;
            gl.color = Qt::black;
            gl.angleDeg = 0.0;
            gl.drawBox = true;
            gridLabels.push_back(gl);
        }
        else // EclipticLine
        {
            int bestIdx = 0;
            double bestDist = std::numeric_limits<double>::max();
            for (int k = 0; k < devicePts.size(); ++k)
            {
                const double d = QLineF(devicePts[k], anchor).length();
                if (d < bestDist) { bestDist = d; bestIdx = k; }
            }
            const int i0 = qBound(0, bestIdx, devicePts.size() - 2);

            const bool p1CloserToAnchor =
                QLineF(devicePts[i0 + 1], anchor).length() < QLineF(devicePts[i0], anchor).length();
            QPointF outward = p1CloserToAnchor ? (devicePts[i0 + 1] - devicePts[i0])
                                                : (devicePts[i0] - devicePts[i0 + 1]);

            double angleDeg = qRadiansToDegrees(std::atan2(outward.y(), outward.x()));
            bool textGrowsNegative = true;
            if (angleDeg > 90.0)  { angleDeg -= 180.0; textGrowsNegative = false; }
            if (angleDeg < -90.0) { angleDeg += 180.0; textGrowsNegative = false; }

            PendingGridLabel gl;
            gl.pos   = anchor;
            gl.text  = line.labelText;
            gl.color = line.color;
            gl.angleDeg = angleDeg;
            gl.drawBox = false;
            gl.growsNegative = textGrowsNegative;
            gridLabels.push_back(gl);
        }
    }

  painter.setFont(QFont("Times New Roman", 7));
    for (const auto& gl : gridLabels)
    {
        painter.save();
        painter.translate(gl.pos);

        if (gl.drawBox)
        {
            const double distLeft   = gl.pos.x() - clipRect.left();
            const double distRight  = clipRect.right()  - gl.pos.x();
            const double distTop    = gl.pos.y() - clipRect.top();
            const double distBottom = clipRect.bottom() - gl.pos.y();
            const bool growLeft = distRight < distLeft;
            const bool growUp   = distBottom < distTop;

            QFontMetricsF fm(painter.font());
            const double textW = fm.horizontalAdvance(gl.text);
            const double textH = fm.height();
            const double pad = 2.0;

            const double boxLeft = growLeft ? -(textW + pad * 2) : pad;
            const double boxTop  = growUp   ? -(textH + pad)     : (pad - textH);

            QRectF box(boxLeft, boxTop, textW + pad * 2, textH);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 200));
            painter.drawRect(box);
            painter.setPen(Qt::black);
            painter.drawText(QPointF(boxLeft + pad, boxTop + fm.ascent()), gl.text);
        }
        else
        {
            painter.rotate(gl.angleDeg);

            QFontMetricsF fm(painter.font());
            const double textW = fm.horizontalAdvance(gl.text);
            const double textX = gl.growsNegative ? -textW : 0.0;
            const double textY = -2.0;

            painter.setPen(gl.color);
            painter.drawText(QPointF(textX, textY), gl.text);
        }

        painter.restore();
    }

    painter.restore();
}

void SkyChartRenderer::drawPolygons(QPainter& painter, const QRectF& pageRect, const SkyScene& scene)
{
    painter.save();
    QPen pen(QColor(150, 150, 150, 150));
    pen.setWidthF(0.5 * m_scale);
    painter.setPen(pen);

    for (const SkyPolygon& poly : scene.polygons)
    {
        if (poly.points.size() < 3) continue;
        QPolygonF qpoly;
        for (const QPointF& p : poly.points)
        {
            qpoly << projectPoint(p, pageRect);
        }
        painter.drawPolygon(qpoly);
    }
    painter.restore();
}

void SkyChartRenderer::drawTexts(QPainter& painter, const QRectF& pageRect, const SkyScene& scene)
{
    painter.save();
    for (const SkyText& t : scene.texts)
    {
        // Antes: se ignoraba t.font y t.color por completo, así que ningún
        // texto (incluidas las etiquetas de planetas de la sección Específica)
        // podía recibir fuente/negrita/cursiva propia. Para las etiquetas
        // "genéricas" (constelaciones, etc.) t.font sigue siendo el QFont()
        // por defecto, así que este cambio no altera su apariencia actual.
        painter.setFont(t.font);
        painter.setPen(t.color.isValid() ? t.color : Qt::black);

        QPointF pos = projectPoint(t.position, pageRect);
        painter.drawText(pos, t.text);
    }
    painter.restore();
}


QColor SkyChartRenderer::starColorForBV(float bv)
{
    if (bv < -0.2f) return QColor(155, 176, 255); // O
    if (bv < 0.0f)  return QColor(170, 191, 255); // B
    if (bv < 0.3f)  return QColor(202, 215, 255); // A
    if (bv < 0.6f)  return QColor(255, 244, 234); // F
    if (bv < 1.0f)  return QColor(255, 210, 161); // G
    if (bv < 1.5f)  return QColor(255, 167,  90); // K
    return QColor(255, 120,  60);                 // M
}

void SkyChartRenderer::drawMilkyWay(QPainter& painter, const QRectF& pageRect,
                                     const SkyScene& scene, const SkyChartExportOptions& options)
{
    // Si el usuario la apagó, no hacemos nada
    if (!scene.state.milkyWayVisible) return;

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // PDF y SVG son formatos VECTORIALES. Si aquí volcáramos siempre
    // scene.milkyWayImage con drawImage(), Qt la embebe como un bloque de
    // píxeles dentro de un documento que, para todo lo demás (estrellas,
    // líneas, texto), es vector puro -> el archivo queda "mezclado":
    // todo se puede reescalar sin perder nitidez salvo ese rectángulo de
    // píxeles de la Vía Láctea. Para esos dos formatos usamos en cambio
    // scene.milkyWayBands -bandas de isodensidad vectoriales, generadas
    // por SkySceneExtractor::captureMilkyWay()-, que son polígonos reales:
    // el PDF/SVG resultante queda 100% vector y escala sin pixelarse.
    //
    // El resto de formatos (JPEG/PNG/TIFF) y el canvas de PreviewDialog al
    // previsualizarlos siguen usando la imagen rasterizada: ahí no hay
    // ninguna mezcla que evitar (el propio archivo de salida ya es
    // píxeles), y el degradado continuo se ve mejor que las bandas.
    const bool isVectorFormat =
        options.exportFormat.compare("PDF", Qt::CaseInsensitive) == 0 ||
        options.exportFormat.compare("SVG", Qt::CaseInsensitive) == 0;

    if (isVectorFormat && !scene.milkyWayBands.isEmpty())
    {
        painter.setPen(Qt::NoPen);
        for (const SkyMilkyWayBand& band : scene.milkyWayBands)
        {
            if (band.contours.isEmpty()) continue;

            QPainterPath path;
            for (const QPolygonF& loop : band.contours)
            {
                if (loop.size() < 3) continue;

                // Las coordenadas de la banda están en UV [0,1]x[0,1]
                // relativo al viewport capturado -el mismo sistema que usa
                // milkyWayImage al mapearse sobre pageRect con drawImage()-,
                // por eso se proyectan igual: fracción lineal de pageRect,
                // NO con projectPoint() (que es para [-1,1]).
                QPolygonF devicePoly;
                devicePoly.reserve(loop.size());
                for (const QPointF& uv : loop)
                {
                    devicePoly << QPointF(pageRect.left() + uv.x() * pageRect.width(),
                                           pageRect.top()  + uv.y() * pageRect.height());
                }
                path.addPolygon(devicePoly);
                path.closeSubpath();
            }

            painter.setBrush(band.color);
            painter.drawPath(path);
        }
    }
    else if (!scene.milkyWayImage.isNull())
    {
        // La dibujamos sobre todo el área del cielo capturado
        painter.drawImage(pageRect, scene.milkyWayImage);
    }

    painter.restore();
}