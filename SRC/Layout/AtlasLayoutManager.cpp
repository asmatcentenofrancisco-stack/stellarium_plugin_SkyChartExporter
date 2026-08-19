#include "AtlasLayoutManager.hpp"
#include "SkyChartRenderer.hpp"
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QPainter>
#include <QtMath>
#include <QPainterPath>

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelMovementMgr.hpp"

AtlasLayoutManager::AtlasLayoutManager()
{
}

QRectF AtlasLayoutManager::buildPageRect(const SkyChartExportOptions& options)
{
    // Conversión de mm a pt (1 mm = 2.83465 pt)
    constexpr double mmToPt = 2.83465;
   double widthMm = 210.0;
    double heightMm = 297.0;

    // Si el usuario especificó un tamaño exacto en píxeles, adaptamos la proporción base del plano
    if (options.imageWidthPx > 0 && options.imageHeightPx > 0)
    {
        heightMm = 297.0;
        widthMm = heightMm * (static_cast<double>(options.imageWidthPx) / options.imageHeightPx);
    }
    else
    {
        if (options.paperSize == "A3") { widthMm = 297.0; heightMm = 420.0; }
        else if (options.paperSize == "A2") { widthMm = 420.0; heightMm = 594.0; }
        else if (options.paperSize == "Carta") { widthMm = 215.9; heightMm = 279.4; }
        else if (options.paperSize == "Legal") { widthMm = 215.9; heightMm = 355.6; }

        if (options.orientation == "Horizontal")
        {
            std::swap(widthMm, heightMm);
        }
    }

    m_pageRect = QRectF(0, 0, widthMm * mmToPt, heightMm * mmToPt);

    // Aplicar márgenes independientes
    double leftPt = options.marginLeft * mmToPt;
    double rightPt = options.marginRight * mmToPt;
    double topPt = options.marginTop * mmToPt;
    double bottomPt = options.marginBottom * mmToPt;

    m_contentRect = QRectF(
        leftPt,
        topPt,
        m_pageRect.width() - (leftPt + rightPt),
        m_pageRect.height() - (topPt + bottomPt)
    );

    return m_pageRect;
}

QRectF AtlasLayoutManager::getContentRect(const SkyChartExportOptions& options) const
{
    Q_UNUSED(options);
    return m_contentRect;
}

QSizeF AtlasLayoutManager::pageSizePx() const
{
    return m_pageRect.size();
}

static void renderAstroLegendLocal(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options, const QRectF& contentRect)
{
    if (!options.showAstroLegend) return;

    bool hasNebulae = scene.state.nebulaeVisible && !scene.nebulae.isEmpty();
    bool hasMilkyWay = scene.state.milkyWayVisible;
    bool hasConstellations = scene.state.constellationLinesVisible;

    if (!hasNebulae && !hasMilkyWay && !hasConstellations) return;

    QFont titleFont("Times New Roman", 8, QFont::Bold);
    QFont itemFont("Times New Roman", 7);

    // Ajustado a 7 items de cielo profundo
    int itemCount = (hasNebulae ? 7 : 0) + (hasMilkyWay ? 1 : 0) + (hasConstellations ? 1 : 0);
    const double itemHeight = 16.0;
    const double boxHeight = 26.0 + (itemCount * itemHeight);
    const double boxWidth = 135.0;

   // La leyenda astronómica (símbolos) va siempre a la izquierda
    double top = contentRect.bottom() - boxHeight - 15.0;
    
    QRectF box(contentRect.left() + 10.0, top, boxWidth, boxHeight);

    painter.save();
    painter.setPen(QPen(Qt::black, 0.5));
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawRect(box);

   painter.setFont(titleFont);
    painter.setPen(Qt::black);
    painter.drawText(QRectF(box.left(), box.top() + 5, box.width(), 12), Qt::AlignHCenter, "— SIMBOLOGÍA —");

    painter.setFont(itemFont);
    
   double currentY = box.top() + 28.0;
    double iconX = box.left() + 18.0;
    double textX = box.left() + 35.0;

    QColor colGal(220, 50, 50);
    QColor colCum(240, 200, 50);
    QColor colNeb(50, 190, 50);
    QColor colOsc(120, 120, 120);

    // Transformación según opciones de exportación
    auto applyColorMode = [&](QColor c) {
        if (options.colorMode == "Monocromo") return QColor(Qt::black);
        if (options.colorMode == "Escala de grises") { int g = qGray(c.rgb()); return QColor(g, g, g); }
        return c;
    };

    colGal = applyColorMode(colGal);
    colCum = applyColorMode(colCum);
    colNeb = applyColorMode(colNeb);
    colOsc = applyColorMode(colOsc);

    if (hasNebulae) {
        // 1. Galaxia
        painter.setPen(QPen(colGal, 0.9, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(iconX, currentY - 4), 5.0, 2.5);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Galaxia");
        currentY += itemHeight;

        // 2. Cúmulo Globular
        painter.setPen(QPen(colCum, 0.8, Qt::SolidLine));
        painter.drawEllipse(QPointF(iconX, currentY - 4), 4.0, 4.0);
        painter.drawLine(QPointF(iconX - 4, currentY - 4), QPointF(iconX + 4, currentY - 4));
        painter.drawLine(QPointF(iconX, currentY - 8), QPointF(iconX, currentY));
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Cúmulo Globular");
        currentY += itemHeight;

        // 3. Cúmulo Abierto
        painter.setPen(QPen(colCum, 0.8, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(iconX, currentY - 4), 4.0, 4.0);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Cúmulo Abierto");
        currentY += itemHeight;

        // 4. Nebulosa
        painter.setPen(QPen(colNeb, 0.9, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(iconX - 4.0, currentY - 8.0, 8.0, 8.0), 2.0, 2.0);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Nebulosa");
        currentY += itemHeight;

        // 5. Nebulosa Planetaria
        painter.setPen(QPen(colNeb, 0.8, Qt::SolidLine));
        painter.drawEllipse(QPointF(iconX, currentY - 4), 4.0, 4.0);
        painter.setBrush(colNeb);
        painter.drawEllipse(QPointF(iconX, currentY - 4), 1.0, 1.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Nebulosa Planetaria");
        currentY += itemHeight;

        // 6. Nebulosa Oscura (Elipse orientada)
        painter.setPen(QPen(colOsc, 0.7, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.save();
        painter.translate(iconX, currentY - 4);
        painter.rotate(-25.0);
        painter.drawEllipse(QPointF(0, 0), 5.0, 2.5);
        painter.restore();
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Nebulosa Oscura");
        currentY += itemHeight;

        // 7. Región Celeste / Nube Estelar
        painter.setPen(QPen(colNeb, 0.8, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.save();
        painter.translate(iconX, currentY - 4);
        painter.rotate(15.0);
        painter.drawEllipse(QPointF(0, 0), 6.0, 3.0);
        painter.restore();
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Región Celeste");
        currentY += itemHeight;
    }

    if (hasConstellations) {
        painter.setPen(QPen(QColor(80, 100, 140), 0.8, Qt::SolidLine));
        painter.drawLine(QPointF(iconX - 5, currentY - 1), QPointF(iconX + 5, currentY - 7));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(80, 100, 140));
        painter.drawEllipse(QPointF(iconX - 5, currentY - 1), 1.0, 1.0);
        painter.drawEllipse(QPointF(iconX + 5, currentY - 7), 1.0, 1.0);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Constelaciones");
        currentY += itemHeight;
    }

    if (hasMilkyWay) {
        painter.setPen(QPen(QColor(100, 120, 150), 0.9, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);
        QPainterPath path1, path2;
        path1.moveTo(iconX - 7, currentY - 2);
        path1.cubicTo(iconX - 3, currentY - 5, iconX + 3, currentY + 1, iconX + 7, currentY - 2);
        path2.moveTo(iconX - 7, currentY - 6);
        path2.cubicTo(iconX - 3, currentY - 9, iconX + 3, currentY - 3, iconX + 7, currentY - 6);
        painter.drawPath(path1);
        painter.drawPath(path2);
        painter.setPen(Qt::black);
        painter.drawText(QPointF(textX, currentY), "Vía Láctea");
    }
    
    painter.restore();
}

void AtlasLayoutManager::renderLayout(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options)
{
    painter.save();

    // Dibujar marco del área imprimible
    QPen framePen(options.colorMode == "Escala de grises" ? Qt::black : QColor(50, 50, 50));
    framePen.setWidthF(1.0);
    painter.setPen(framePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_contentRect);

    renderHeader(painter, options);
    renderFooter(painter, scene, options);
   renderLegends(painter, scene, options);
    renderSpectralLegend(painter, scene, options); // Pasamos scene para el reacomodo inteligente
    renderAstroLegendLocal(painter, scene, options, m_contentRect);
    renderCoordinateSystemLabel(painter, scene, options);

   // Identificación de Stellarium y Licencia Open Source (alineado a la izquierda)
    painter.save();
    // Reducimos el tamaño de letra a 5 para que sea una nota de referencia mínima y discreta
    QFont watermarkFont("Times New Roman", 6, QFont::Normal, true); 
    painter.setFont(watermarkFont);
    
    // Usamos un gris sutil para que no destaque más que el plano
    painter.setPen(options.colorMode == "Monocromo" ? Qt::black : QColor(130, 130, 130)); 
    QString textWatermark = "@Stellarium 26.1 - Licencia GNU GPL";
    
    // Usamos exactamente la misma referencia de margen izquierdo que el cajetín de magnitudes.
    // Así obligamos a que el texto inicie en la misma línea vertical (izquierda) que el cajetín.
    double xPos = qMax(m_contentRect.left() + 10.0, m_pageRect.left() + kMinPageEdgeMargin);
    
    // Posición inferior, respetando el borde del recuadro del plano
    double yPos = m_contentRect.bottom() - 3.0; 
    
    painter.drawText(QPointF(xPos, yPos), textWatermark);
    painter.restore();

    if (options.showNorthArrow)
    {
        renderNorthArrow(painter, m_contentRect, scene);
    }

    painter.restore();
}

void AtlasLayoutManager::renderHeader(QPainter& painter, const SkyChartExportOptions& options)
{
    double currentY = m_contentRect.top() + 15.0;

    if (!options.title.isEmpty())
    {
        painter.setFont(options.titleFont);
        QFontMetricsF fm(painter.font());
        const double textWidth = fm.horizontalAdvance(options.title);

        QRectF titleBox(m_contentRect.center().x() - textWidth / 2.0 - 12.0,
                         currentY, textWidth + 24.0, fm.height() + 10.0);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 235));
        painter.drawRect(titleBox);

        painter.setPen(Qt::black);
        painter.drawText(titleBox, Qt::AlignCenter, options.title);
        currentY += fm.height() + 14.0;
    }

  if (!options.subtitle.isEmpty())
    {
        painter.setFont(options.subtitleFont);
        QFontMetricsF fm(painter.font());
        const double textWidth = fm.horizontalAdvance(options.subtitle);


        QRectF subBox(m_contentRect.center().x() - textWidth / 2.0 - 10.0,
                       currentY, textWidth + 20.0, fm.height() + 8.0);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 220));
        painter.drawRect(subBox);

        painter.setPen(QColor(80, 80, 80));
        painter.drawText(subBox, Qt::AlignCenter, options.subtitle);
    }
}

void AtlasLayoutManager::renderFooter(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options)
{
    Q_UNUSED(options);

    QFont line1Font("Times New Roman", 9, QFont::Bold);
    QFont line2Font("Times New Roman", 8);

    QString line1 = scene.observer.observatoryName.isEmpty()
                    ? QString("Observatorio no definido")
                    : scene.observer.observatoryName;

    QString line2 = QString("Lat: %1°  Lon: %2°")
                        .arg(scene.observer.latitude, 0, 'f', 2)
                        .arg(scene.observer.longitude, 0, 'f', 2);

    painter.setFont(line1Font);
    QFontMetricsF fm1(painter.font());
    painter.setFont(line2Font);
    QFontMetricsF fm2(painter.font());


    const double w = qMax(fm1.horizontalAdvance(line1), fm2.horizontalAdvance(line2)) + 24.0;
    const double h = fm1.height() + fm2.height() + 10.0;

    QRectF box(m_contentRect.center().x() - w / 2.0,
               m_contentRect.bottom() - h - 8.0, w, h);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 230));
    painter.drawRect(box);

    painter.setPen(Qt::black);
    painter.setFont(line1Font);
    painter.drawText(QRectF(box.left(), box.top() + 2, box.width(), fm1.height()),
                      Qt::AlignCenter, line1);

    painter.setPen(QColor(90, 90, 90));
    painter.setFont(line2Font);
    painter.drawText(QRectF(box.left(), box.top() + fm1.height() + 4, box.width(), fm2.height()),
                      Qt::AlignCenter, line2);
}



void AtlasLayoutManager::renderLegends(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options)
{
    Q_UNUSED(scene);
    if (!options.showMagnitudeLegend) return;

    QFont titleFont("Times New Roman", 9, QFont::Bold);
    QFont itemFont("Times New Roman", 8);

    const double boxHeight = 55.0;
    const double boxWidth = 148.0; // Ancho reducido para quitar el exceso de margen lateral
    
    // La leyenda de magnitudes va firmemente a la DERECHA
    const double right = qMin(m_contentRect.right() - 10.0, m_pageRect.right() - kMinPageEdgeMargin);
    const double top   = qMin(m_contentRect.bottom() - boxHeight - 15.0,
                               m_pageRect.bottom() - boxHeight - kMinPageEdgeMargin);
    QRectF box(right - boxWidth, top, boxWidth, boxHeight);

    painter.setPen(QPen(Qt::black, 0.5));
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawRect(box);

    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    painter.drawText(QRectF(box.left(), box.top() + 2, box.width(), 16), Qt::AlignHCenter, "— MAGNITUD VISUAL —");

    painter.setFont(itemFont);
    const int mags[] = {0, 1, 2, 3, 4, 5, 6};
    const int count = 7;
    const double spacing = 20.0; // Espaciado horizontal optimizado al nuevo ancho
    const double rowWidth = spacing * (count - 1);

    double x = box.center().x() - rowWidth / 2.0;

    const double baseRadius = 4.0;
    const double decay = 0.82;

    QFontMetricsF fm(itemFont);
    const double circleY = box.top() + 26.0;
    const double textY = box.top() + 44.0; // Espacio vertical claro y armónico respecto al círculo

    // Transformación según opciones de exportación de color
    auto applyColorMode = [&](QColor c) {
        if (options.colorMode == "Monocromo") return QColor(Qt::black);
        if (options.colorMode == "Escala de grises") { int g = qGray(c.rgb()); return QColor(g, g, g); }
        return c;
    };

    for (int m : mags)
    {
        double r = qMax(0.8, baseRadius * std::pow(decay, m));
        QColor circleColor = applyColorMode(Qt::black);
        
        painter.setPen(QPen(circleColor, 0.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(x, circleY), r, r);
        
        painter.setPen(circleColor);
        QString numStr = QString::number(m);
        double textW = fm.horizontalAdvance(numStr);
        painter.drawText(QPointF(x - textW / 2.0, textY), numStr); // Centrado matemático exacto
        
        x += spacing;
    }
}

void AtlasLayoutManager::renderSpectralLegend(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options)
{
    if (!options.showSpectralLegend) return;

    bool hasNebulae = scene.state.nebulaeVisible && !scene.nebulae.isEmpty();
    bool hasMilkyWay = scene.state.milkyWayVisible;
    bool hasConstellations = scene.state.constellationLinesVisible;
    bool astroLegendVisible = options.showAstroLegend && (hasNebulae || hasMilkyWay || hasConstellations);
    bool magLegendVisible = options.showMagnitudeLegend;

    bool isVertical = (astroLegendVisible && magLegendVisible);

    struct SpectralEntry { QString letter; float bv; };
    static const SpectralEntry entries[] = {
        {"O", -0.30f}, {"B", -0.10f}, {"A", 0.15f},
        {"F", 0.45f},  {"G", 0.80f},  {"K", 1.25f}, {"M", 1.60f}
    };
    const int count = 7;

    if (isVertical)
    {
        // === FORMATO VERTICAL ===
        QFont titleFont("Times New Roman", 7, QFont::Bold);
        QFont itemFont("Times New Roman", 7);
        const double itemHeight = 14.0;
        const double w = 45.0; 
        const double h = 20.0 + (count * itemHeight);
        const double gap = 10.0;

        const double magBoxHeight = 55.0;
        double rightPos = qMin(m_contentRect.right() - 10.0, m_pageRect.right() - kMinPageEdgeMargin);
        double x = rightPos - w;
        double y;

        // Validar si hay coordenadas visibles en la escena actual
        bool hasCoordinates = (scene.state.coordinateSystem != SkyCoordinateSystem::None);

        if (hasCoordinates)
        {
            // Si hay coordenadas, la leyenda espectral sube para ubicarse sobre el rótulo
            QFont coordFont("Times New Roman", 8, QFont::Normal, true);
            QFontMetricsF coordFm(coordFont);
            const double coordH = coordFm.height() + 8.0;

            double coordTop = qMin(m_contentRect.bottom() - magBoxHeight - gap - coordH - 15.0,
                                   m_pageRect.bottom() - magBoxHeight - gap - coordH - kMinPageEdgeMargin);
            y = coordTop - gap - h;
        }
        else
        {
            // Si NO hay coordenadas, baja y se apoya directamente sobre la leyenda de magnitudes
            y = qMin(m_contentRect.bottom() - magBoxHeight - h - gap - 15.0,
                     m_pageRect.bottom() - magBoxHeight - h - gap - kMinPageEdgeMargin);
        }

        QRectF box(x, y, w, h);

        painter.setPen(QPen(Qt::black, 0.5));
        painter.setBrush(QColor(255, 255, 255, 235));
        painter.drawRect(box);

        painter.setFont(titleFont);
        painter.setPen(Qt::black);
        painter.drawText(QRectF(box.left(), box.top() + 4, box.width(), 12), Qt::AlignHCenter, "— T. ESP. —");

        painter.setFont(itemFont);
        double currentY = box.top() + 24.0;
        double iconX = box.left() + 14.0;
        double textX = box.left() + 24.0;

        for (const auto& e : entries)
        {
            QColor c = SkyChartRenderer::starColorForBV(e.bv);
            if (options.colorMode == "Monocromo") c = Qt::black;
            else if (options.colorMode == "Escala de grises") { int gray = qGray(c.rgb()); c = QColor(gray, gray, gray); }

            painter.setPen(QPen(Qt::black, 0.4));
            painter.setBrush(c);
            painter.drawEllipse(QPointF(iconX, currentY - 3), 4.0, 4.0);

            painter.setPen(Qt::black);
            painter.drawText(QPointF(textX, currentY + 2), e.letter);
            currentY += itemHeight;
        }
    }
    else
    {
        // === FORMATO HORIZONTAL ===
        QFont titleFont("Times New Roman", 9, QFont::Bold);
        QFont itemFont("Times New Roman", 8);
        const double w = 148.0;
        const double h = 55.0;

        double x, y;
        y = qMin(m_contentRect.bottom() - h - 15.0, m_pageRect.bottom() - h - kMinPageEdgeMargin);

        if (magLegendVisible) {
            x = qMax(m_contentRect.left() + 10.0, m_pageRect.left() + kMinPageEdgeMargin);
        } else {
            x = qMin(m_contentRect.right() - 10.0, m_pageRect.right() - kMinPageEdgeMargin) - w;
        }

        QRectF box(x, y, w, h);

        painter.setPen(QPen(Qt::black, 0.5));
        painter.setBrush(QColor(255, 255, 255, 235));
        painter.drawRect(box);

        painter.setFont(titleFont);
        painter.setPen(Qt::black);
        painter.drawText(QRectF(box.left(), box.top() + 2, box.width(), 16), Qt::AlignHCenter, "— TIPO ESPECTRAL —");

        painter.setFont(itemFont);
        const double spacing = 20.0;
        const double rowWidth = spacing * (count - 1);
        double currentX = box.center().x() - rowWidth / 2.0;
        
        const double circleY = box.top() + 26.0;
        const double textY = box.top() + 44.0;
        QFontMetricsF fm(itemFont);

        for (const auto& e : entries)
        {
            QColor c = SkyChartRenderer::starColorForBV(e.bv);
            if (options.colorMode == "Monocromo") c = Qt::black;
            else if (options.colorMode == "Escala de grises") { int gray = qGray(c.rgb()); c = QColor(gray, gray, gray); }

            painter.setPen(QPen(Qt::black, 0.4));
            painter.setBrush(c);
            painter.drawEllipse(QPointF(currentX, circleY), 4.0, 4.0);

            painter.setPen(Qt::black);
            double textW = fm.horizontalAdvance(e.letter);
            painter.drawText(QPointF(currentX - textW / 2.0, textY), e.letter);
            
            currentX += spacing;
        }
    }
}

void AtlasLayoutManager::renderNorthArrow(QPainter& painter, const QRectF& rect, const SkyScene& scene)
{
    Q_UNUSED(scene);
    painter.save();
    
    // Posición de la rosa de los vientos en la esquina superior derecha del área de contenido
    const double cx = rect.right() - 40.0;
    const double cy = rect.top() + 40.0;
    const double radius = 22.0; // Radio del círculo blanco protector

    // 1. Círculo blanco de fondo para evitar superposición con estrellas u objetos celestes
    painter.setPen(QPen(Qt::black, 1.0));
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(cx, cy), radius, radius);

    // 2. Orientación cartográfica dinámica según la vista actual del observador
    double rotationAngle = scene.state.northRotationAngle;

    // Trasladamos el origen al centro de la rosa y aplicamos la rotación
    painter.translate(cx, cy);
    painter.rotate(rotationAngle);

    const double armLen = 18.0;
    const double halfWidth = 5.0;

    // 3. Dibujo de los 4 triángulos principales (mitad negro, mitad blanco)
    
    // --- PUNTO NORTE (N) ---
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black); // Mitad izquierda negra
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(0, -armLen) << QPointF(-halfWidth, 0));
    painter.setPen(QPen(Qt::black, 0.4));
    painter.setBrush(Qt::white); // Mitad derecha blanca
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(0, -armLen) << QPointF(halfWidth, 0));

    // --- PUNTO SUR (S) ---
    painter.setPen(QPen(Qt::black, 0.4));
    painter.setBrush(Qt::white); // Mitad izquierda blanca
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(0, armLen) << QPointF(-halfWidth, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black); // Mitad derecha negra
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(0, armLen) << QPointF(halfWidth, 0));

    // --- PUNTO ESTE (E) ---
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black); // Mitad superior negra
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(armLen, 0) << QPointF(0, -halfWidth));
    painter.setPen(QPen(Qt::black, 0.4));
    painter.setBrush(Qt::white); // Mitad inferior blanca
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(armLen, 0) << QPointF(0, halfWidth));

    // --- PUNTO OESTE (W) ---
    painter.setPen(QPen(Qt::black, 0.4));
    painter.setBrush(Qt::white); // Mitad superior blanca
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(-armLen, 0) << QPointF(0, -halfWidth));
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black); // Mitad inferior negra
    painter.drawPolygon(QPolygonF() << QPointF(0, 0) << QPointF(-armLen, 0) << QPointF(0, halfWidth));

    // 4. Etiquetas cardinales (N, S, E, W) alineadas con las puntas
    QFont font("Times New Roman", 8, QFont::Bold);
    painter.setFont(font);

    // Envolvente independiente y dinámica exclusiva para la letra N
    QRectF nBox(-9, -armLen - 15, 18, 14);
    painter.setPen(QPen(Qt::black, 0.6));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(nBox, 2.0, 2.0);

    painter.setPen(Qt::black);
    painter.drawText(nBox, Qt::AlignCenter, "N");
    

    painter.restore();
}

void AtlasLayoutManager::renderCoordinateSystemLabel(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options)
{
    if (scene.state.coordinateSystem == SkyCoordinateSystem::None)
        return;

    QString label = (scene.state.coordinateSystem == SkyCoordinateSystem::Equatorial)
                        ? QString("Coordenadas Ecuatoriales")
                        : QString("Coordenadas Azimutales");

    QFont font("Times New Roman", 8, QFont::Normal, true);
    painter.setFont(font);
    QFontMetricsF fm(painter.font());

    const double w = fm.horizontalAdvance(label) + 16.0;
    const double h = fm.height() + 8.0;
    const double gap = 3.0; // Distancia inferior reducida para que el rótulo actúe como un renglón compacto sobre la leyenda horizontal

    bool magLegendVisible = options.showMagnitudeLegend;
    bool specLegendVisible = options.showSpectralLegend;

    // Escenario 3 por defecto: Ninguna leyenda (ni magnitud ni espectral en la derecha) -> sobre el margen del papel
    double boxTop = qMin(m_contentRect.bottom() - h - 15.0, m_pageRect.bottom() - h - kMinPageEdgeMargin); 

    if (magLegendVisible) {
        // Escenario 1: Está la leyenda de magnitud -> el rótulo va SOBRE la magnitud
        const double magBoxHeight = 55.0;
        boxTop = qMin(m_contentRect.bottom() - magBoxHeight - gap - h - 15.0,
                      m_pageRect.bottom() - magBoxHeight - gap - h - kMinPageEdgeMargin);
    } else if (specLegendVisible) {
        // Escenario 2: No está la de magnitud pero está la espectral horizontal -> sobre la espectral horizontal
        const double specBoxHeight = 55.0;
        boxTop = qMin(m_contentRect.bottom() - specBoxHeight - gap - h - 15.0,
                      m_pageRect.bottom() - specBoxHeight - gap - h - kMinPageEdgeMargin);
    }

    QRectF box(m_contentRect.right() - w - 10.0, boxTop, w, h);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawRect(box);

    painter.setPen(Qt::black);
    painter.drawText(box, Qt::AlignCenter, label);
}