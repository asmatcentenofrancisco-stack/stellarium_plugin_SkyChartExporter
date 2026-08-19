/*
 * SkyChartExporter Plugin
 * Stellarium 26.1
 *
 * SkySceneExtractor.hpp
 *
 * Extrae el estado actual de Stellarium y construye una
 * escena independiente (SkyScene).
 *
 * No dibuja.
 * No exporta PDF.
 * No conoce el diálogo.
 */

#ifndef SKYSCENEEXTRACTOR_HPP
#define SKYSCENEEXTRACTOR_HPP

#include "SkyChartExporterOptions.hpp"

#include <QObject>
#include <QString>
#include <QPointF>
#include <QColor>

#include "SkyScene.hpp"
#include "VecMath.hpp"
#include "NebulaMgr.hpp"
#include "StelObject.hpp"
#include "Star.hpp"
#include "Nebula.hpp"
#include "LandscapeMgr.hpp"
#include "StelCore.hpp"


// Declaraciones anticipadas
class StelCore;
class StelProjector;
class StelObjectMgr;
class StelModuleMgr;
class StelSkyDrawer;
class StelMovementMgr;

class StarMgr;
class SolarSystem;
class NebulaMgr;
class ConstellationMgr;
class StelObject;

class GridLinesMgr;

class SkySceneExtractor : public QObject
{
    Q_OBJECT

public:

    explicit SkySceneExtractor(QObject* parent = nullptr);

    ~SkySceneExtractor() override = default;

    //----------------------------------------------------
    // Método principal
    //----------------------------------------------------

    bool extractScene(SkyScene& scene, const SkyChartExportOptions& options = SkyChartExportOptions());

    SkySceneState captureCurrentState();


private:

  //----------------------------------------------------
// Captura de la escena
//----------------------------------------------------

void captureModuleState(SkyScene& scene);

void captureStars(SkyScene& scene);

void captureSolarSystem(SkyScene& scene);

void captureNebulae(SkyScene& scene);

void captureConstellations(SkyScene& scene);

void captureGrid(SkyScene& scene);

void captureMilkyWay(SkyScene& scene);

void captureHorizon(SkyScene& scene); 

void captureLabels(SkyScene& scene);

void buildEquatorialGridLines(SkyScene& scene);

void buildAzimuthalGridLines(SkyScene& scene);

void buildConstellationLines(SkyScene& scene);

bool isAboveHorizon(const Vec3d& j2000Pos) const;

void buildHorizonLine(SkyScene& scene);

    //----------------------------------------------------
    // Utilidades
    //----------------------------------------------------
    // La versión const es obligatoria para utilidades matemáticas que no modifican la clase
bool projectJ2000(const Vec3d& position, QPointF& screenPoint) const;

QColor starColor(float bv) const;

QColor spectralColor(const QString& spectral) const;

QString formatLatitude(double latitude) const;

QString formatLongitude(double longitude) const;

QString formatAltitude(double altitude) const;

QString formatJulianDay(double jd) const;

    //----------------------------------------------------
// Datos internos
//----------------------------------------------------

StelCore* m_core = nullptr;

StarMgr* m_starMgr = nullptr;

ConstellationMgr* m_constMgr = nullptr;

NebulaMgr* m_nebulaMgr = nullptr;

SolarSystem* m_solarSystem = nullptr;

GridLinesMgr* m_gridMgr = nullptr;

StelObjectMgr* m_objectMgr = nullptr;
 
LandscapeMgr* m_landscapeMgr = nullptr; 

// NUEVO: réplica del criterio de etiqueta que YA usa Stellarium en
    // pantalla (nombre común vs. designación Bayer/Flamsteed), sin
    // opciones del plugin de por medio.
    QString resolveStarLabel(const StelObjectP& object) const;

    // NUEVO: geometría de la eclíptica, mismo patrón que buildHorizonLine()
    void buildEclipticLine(SkyScene& scene, bool ofDate);
  
};

#endif // SKYSCENEEXTRACTOR_HPP