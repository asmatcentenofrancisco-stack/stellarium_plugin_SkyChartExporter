/*
 * SkyChartExporter Plugin
 * SkyChartRenderer.hpp
 *
 * Motor de renderizado vectorial del mapa celeste.
 *
 * Esta clase NO consulta Stellarium.
 * Únicamente dibuja un SkyScene previamente extraído por
 * SkySceneExtractor.
 */

#ifndef SKYCHARTRENDERER_HPP
#define SKYCHARTRENDERER_HPP

#include <QPainter>
#include <QRectF>

#include "SkyScene.hpp"
#include "SkyChartExporterOptions.hpp"

class SkyChartRenderer
{
public:

    //-----------------------------------------------------------------
    // Construcción
    //-----------------------------------------------------------------

    SkyChartRenderer();

    ~SkyChartRenderer() = default;

    //-----------------------------------------------------------------
    // Renderizado principal
    //-----------------------------------------------------------------

    bool render(QPainter& painter,
            const QRectF& pageRect,
            const SkyScene& scene,
            const SkyChartExportOptions& options);

    static double starRadiusForMagnitude(double magnitude, double limitingMagnitude, double scale);

    static double bodyRadiusForMagnitude(double magnitude, double limitingMagnitude, double scale);

    static bool starIsFilled(double magnitude);

    static QColor starColorForBV(float bv);


private:

//-----------------------------------------------------------------
// Primitivas gráficas
//-----------------------------------------------------------------

void drawBackground(
        QPainter& painter,
        const QRectF& pageRect,
        const SkyScene& scene);

void drawGrid(
        QPainter& painter,
        const QRectF& pageRect,
        const SkyScene& scene);

void drawMilkyWay(QPainter& painter, 
                   const QRectF& pageRect,
                   const SkyScene& scene, 
                   const SkyChartExportOptions& options);

void drawStars(QPainter& painter,
               const QRectF& pageRect,
               const SkyScene& scene,
               const SkyChartExportOptions& options);

void drawSolarSystem(
        QPainter& painter,
        const QRectF& pageRect,
        const SkyScene& scene);

void drawNebulae(
        QPainter& painter,
        const QRectF& pageRect,
        const SkyScene& scene);

void drawPolylines(QPainter& painter, const QRectF& sceneRect, const QRectF& clipRect, const SkyScene& scene);

void drawPolygons(
        QPainter& painter,
        const QRectF& pageRect,
        const SkyScene& scene);

void drawTexts(
        QPainter& painter,
        const QRectF& pageRect,
        const SkyScene& scene);

void drawMoonPhase(
        QPainter& painter, 
        const QPointF& pos, 
        double r, 
        double illuminatedFraction,
         double limbAngleDeg, 
        const QColor& color);

//-----------------------------------------------------------------
// Conversión de coordenadas
//-----------------------------------------------------------------

QPointF projectPoint(
        const QPointF& normalizedPoint,
        const QRectF& pageRect) const;

double scaleRadius(
        double radius) const;

//-----------------------------------------------------------------
// Configuración gráfica
//-----------------------------------------------------------------

void setupPainter(
        QPainter& painter);



//--------------------------------------------------------------
// Estado interno del renderizador
//--------------------------------------------------------------

double m_scale = 1.0;

// Área completa de dibujo del documento PDF.
QRectF m_pageRect;

// Cruce contra el pageRect REAL (coordenadas de página, no normalizadas)
static bool findDeviceEdgeCrossing(const QVector<QPointF>& devicePts,
                                    const QRectF& pageRect, QPointF& out);

// Punto visible más cercano al borde, con margen fijo en pt (no normalizado)
static QPointF closestVisiblePoint(const QVector<QPointF>& devicePts, const QRectF& clipRect, int& outIndex);


};



#endif // SKYCHARTRENDERER_HPP