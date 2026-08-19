#pragma once

#include "SkyChartExporterOptions.hpp"

#include <QVector>
#include <QPointF>
#include <QPolygonF>
#include <QSizeF>
#include <QString>
#include <QColor>
#include <QDateTime>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QImage>


// ==========================================================
// ENUMS
// ==========================================================

enum class SkyCoordinateSystem
{
    None,        // Ninguna cuadrícula activa: no debe dibujarse ningún rótulo
    Horizontal,  // Cuadrícula azimutal (Az/Alt) activa
    Equatorial   // Cuadrícula ecuatorial (RA/Dec) activa
};

enum class SkyLayer
{
    Stars,
    SolarSystem,
    Nebulae,
    Grid,
    Labels
};

enum class SkyPolylineType {
    GridLine,           // Cuadrículas ecuatorial / horizontal
    HorizonLine,        // Línea del horizonte
    ConstellationLine,  // Líneas de constelaciones
    EquatorLine,        // Línea ecuatorial celeste
    EclipticLine,       // Eclíptica
    MeridianLine,       // Meridiano
    Generic             // Fallback para cualquier otra
};


// ==========================================================
// PRIMITIVAS GRÁFICAS (ESTRUCTURAS FUNDAMENTALES)
// ==========================================================

enum class SkySymbolType
{
    Planet,
    Galaxy,
    OpenCluster,
    GlobularCluster,
    PlanetaryNebula,
    DarkNebula,
    DiffuseNebula,
    EmissionNebula, // NUEVO: Nebulosas de Emisión
    SkyRegion       // NUEVO: Regiones Celestes
};

struct SkyPoint
{
    QString id;
    QString label;
    QPointF position;
    double radius = 1.0;
    float magnitude = 99.0f;
    QColor color; // El renderizador necesita leer el color directamente
    QPen pen;
    QBrush brush;
};

struct SkySymbol
{
    SkySymbolType type;
    QString name;
    QPointF position;
    double scale = 1.0;
    double scaleY = 0.0;     // <-- Necesario para elipses de Nubes Estelares
    double rotation = 0.0;
    bool   waning = false;   // solo relevante para la Luna
    QColor color;
    SkyLayer layer;
    QPointF sunDirection;
    bool    hasSunDirection = false;
};

struct SkyText
{
    QString text;
    QPointF position;
    QColor color;
    QFont font;
};

struct SkyPolyline {
    QVector<QPointF> points;
    int category = 0;
    SkyPolylineType type = SkyPolylineType::Generic;  // <-- NUEVO
    QColor color = QColor(100, 100, 100);             // <-- NUEVO (por si quieres override)
    float width = 0.35f;                              // <-- NUEVO
     
    Qt::PenStyle style = Qt::SolidLine;
    QString labelText;

    // ------------------------------------------------------
    // METADATOS DE ETIQUETA (reemplaza el intento anterior con NDC)
    // ------------------------------------------------------
    bool isMeridianLine     = false; // true = RA/Az (línea de "arriba a abajo") -> etiqueta arriba
    bool truncatedByHorizon = false; // la línea no llega al marco: la cortó el horizonte
};


struct SkyPolygon
{
    QVector<QPointF> points;
    QPen pen;
    QBrush brush;
};

// ==========================================================
// VÍA LÁCTEA — REPRESENTACIÓN VECTORIAL (bandas de isodensidad)
// ==========================================================
// scene.milkyWayImage (más abajo) sigue siendo la representación RASTER,
// usada por los formatos de píxeles (JPEG/PNG/TIFF) y por el canvas del
// PreviewDialog cuando se está previsualizando uno de esos formatos.
//
// scene.milkyWayBands es la representación VECTORIAL equivalente, pensada
// para PDF/SVG: en vez de una nube de píxeles con degradado continuo, es
// un pequeño conjunto de "bandas" -de la más tenue/grande a la más
// brillante/pequeña- cada una formada por uno o más contornos cerrados
// (polígonos) con un color plano. Al dibujarlas superpuestas (tenue primero,
// brillante encima) el compositing normal de QPainter reproduce un
// degradado escalonado que se acerca visualmente al raster, pero el
// resultado son trazos 100% vectoriales: no hay píxeles embebidos en un
// documento que para todo lo demás (estrellas, líneas, texto) es vector.
//
// Los puntos de cada contorno están en espacio UV [0,1]x[0,1] relativo al
// viewport capturado -el mismo sistema que usa milkyWayImage al dibujarse
// con drawImage(pageRect, ...)-, NO en coordenadas normalizadas [-1,1]
// como el resto de la geometría de la escena (stars, planets, etc.).
struct SkyMilkyWayBand
{
    QVector<QPolygonF> contours; // uno o más lóbulos cerrados de esta banda
    QColor color;                 // color plano (incluye alfa) de la banda
};

// ==========================================================
// OBSERVER
// ==========================================================

struct SkyObserverInfo
{
    QString observatoryName;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;

    QDateTime localDateTime;
    double julianDay = 0.0;
};

// ==========================================================
// VIEW
// ==========================================================

struct SkyViewParameters
{
    double fieldOfView = 0.0;
    double limitingMagnitude = 6.0;
};

// ==========================================================
// STATE (CONTROLES DE RENDER)
// ==========================================================

struct SkySceneState
{
    bool starsVisible = true;
    bool planetsVisible = true;
    bool nebulaeVisible = true;
    bool atmosphereVisible = true; 
    bool milkyWayVisible = true;
    int milkyWayAlpha = 150; // NUEVO

    bool constellationLinesVisible = false;
    bool constellationLabelsVisible = false;
    bool constellationBoundariesVisible = false;

    bool equatorialGridVisible = false;
    bool spectralLegendVisible;
    bool horizontalGridVisible = false;

    bool equatorVisible = false;
    bool eclipticVisible = false;
    bool eclipticJ2000Visible = false;
    bool meridianVisible = false;
    bool deepSkyLabelsVisible = false;

    bool starLabelsVisible = false;
    bool planetLabelsVisible = false;
    
    bool horizonLineVisible = true; 

    bool grayscale = false;

    float planetScale = 1.0f;

    bool cardinalPointsVisible = false;

    double northRotationAngle = 0.0;

    SkyCoordinateSystem coordinateSystem = SkyCoordinateSystem::None;
};

// ==========================================================
// STATISTICS
// ==========================================================

struct SkyStatistics
{
    int starCount = 0;
    int planetCount = 0;
    int nebulaCount = 0;
};

// ==========================================================
// SCENE PRINCIPAL (VERSIÓN LIMPIA)
// ==========================================================

struct SkyScene
{
    SkySceneState state;
    SkyObserverInfo observer;
    SkyViewParameters view;
    SkyStatistics statistics;

    QSizeF viewportSize;

    QList<SpecificAstroRule> specificTargets;

    // ------------------------------------------------------
    // GEOMETRÍA (TIPOS COMPLETOS -> OK PARA Qt containers)
    // ------------------------------------------------------

    QVector<SkyPoint> stars;
    QVector<SkySymbol> planets;
    QVector<SkySymbol> nebulae;
    QVector<SkyText> texts;
    QVector<SkyPolyline> polylines;
    QVector<SkyPolygon> polygons;
    QImage milkyWayImage;                 // representación RASTER (JPEG/PNG/TIFF, preview raster)
    QVector<SkyMilkyWayBand> milkyWayBands; // representación VECTORIAL (PDF/SVG, preview vectorial)

    // ======================================================
    // UTILIDADES
    // ======================================================

    bool isEmpty() const
    {
        return stars.isEmpty() &&
               planets.isEmpty() &&
               nebulae.isEmpty();
    }

    void clear()
{
    stars.clear();
    planets.clear();
    nebulae.clear();
    texts.clear();
    polylines.clear();
    polygons.clear();
    milkyWayImage = QImage();
    milkyWayBands.clear();
}

    void updateStatistics()
    {
        statistics.starCount   = stars.size();
        statistics.planetCount = planets.size();
        statistics.nebulaCount = nebulae.size();
    }

    int primitiveCount() const
    {
        return stars.size()
             + planets.size()
             + nebulae.size()
             + texts.size();
    }
};