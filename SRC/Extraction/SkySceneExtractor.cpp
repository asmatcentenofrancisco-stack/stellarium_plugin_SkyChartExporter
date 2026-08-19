#include "SkySceneExtractor.hpp"
#include "SkyScene.hpp"
#include "SkyChartRenderer.hpp"


#include "StelFileMgr.hpp"
#include "StelSkyCultureMgr.hpp"
#include "StelMovementMgr.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QImage>
#include <QColor>
#include <QPolygonF>
#include <QLineF>
#include <QMultiMap>
#include <cmath>


#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelTranslator.hpp"   

#include "StarMgr.hpp"
#include "Star.hpp"
#include "ConstellationMgr.hpp"
#include "NebulaMgr.hpp"
#include "Nebula.hpp"
#include "SolarSystem.hpp"
#include "LandscapeMgr.hpp"
#include "GridLinesMgr.hpp"
#include "StelUtils.hpp"
#include <QRegularExpression>

#include "StelProjector.hpp"
#include "StelProjectorClasses.hpp"
#include "StelMovementMgr.hpp"
#include "Nebula.hpp"
#include "Planet.hpp"


#include <QDateTime>
#include <QDebug>
#include <cmath>


// Conversión grados -> radianes, usada en varios extractores de este archivo
// (captureSolarSystem, captureNebulae, buildHorizonLine, buildConstellationLines).
// Declarada una sola vez a nivel de archivo para evitar duplicados o errores
// de ámbito entre funciones.
constexpr double DEG2RAD = 0.017453292519943295;

//*********************************************************************
// Secciòn I: Constructor
//*********************************************************************

SkySceneExtractor::SkySceneExtractor(QObject* parent)
    : QObject(parent)
{
}


//*********************************************************************
// Extrae completamente la escena actual de Stellarium
//*********************************************************************

bool SkySceneExtractor::extractScene(SkyScene& scene, const SkyChartExportOptions& options)
{
    // Usamos macros de Stellarium, no templates personalizados
    auto* core          = StelApp::getInstance().getCore();
    auto* starMgr       = GETSTELMODULE(StarMgr);
    auto* constMgr      = GETSTELMODULE(ConstellationMgr);
    auto* nebMgr        = GETSTELMODULE(NebulaMgr);
    auto* solarSystem   = GETSTELMODULE(SolarSystem);
    auto* landscapeMgr  = GETSTELMODULE(LandscapeMgr);
    auto* gridMgr       = GETSTELMODULE(GridLinesMgr);
    auto* objMgr        = GETSTELMODULE(StelObjectMgr);

    if (!core || !starMgr || !nebMgr || !objMgr)
        return false;
      
      scene.clear(); 
    //----------------------------------------------------------
    // Obtener módulos principales (Usando macros estándar)
    //----------------------------------------------------------

    m_core          = core;
    m_starMgr       = GETSTELMODULE(StarMgr);
    m_constMgr      = GETSTELMODULE(ConstellationMgr);
    m_nebulaMgr     = GETSTELMODULE(NebulaMgr);
    m_solarSystem   = GETSTELMODULE(SolarSystem);
    m_landscapeMgr  = GETSTELMODULE(LandscapeMgr);
    m_gridMgr       = GETSTELMODULE(GridLinesMgr);
    m_objectMgr     = GETSTELMODULE(StelObjectMgr);

    //----------------------------------------------------------
    // Información del observador
    //----------------------------------------------------------

    const StelLocation& location =
            core->getCurrentLocation();

    scene.observer.observatoryName =
            location.name;

    // Código corregido
    scene.observer.latitude = location.getLatitude();
    scene.observer.longitude = location.getLongitude();

    scene.observer.altitude =
            location.altitude;

    scene.observer.localDateTime =
            QDateTime::currentDateTime();

    scene.observer.julianDay =
            core->getJD();

    //----------------------------------------------------------
    // Parámetros científicos
    //----------------------------------------------------------

    scene.view.fieldOfView =
            core->getMovementMgr()->getCurrentFov();

    scene.view.limitingMagnitude = options.limitingMagnitude;
    scene.state.milkyWayVisible = options.showMilkyWay;
    scene.state.milkyWayAlpha = options.milkyWayAlpha;

    // Transfiere los astros específicos seleccionados desde la interfaz hacia la escena de análisis
    scene.specificTargets = options.specificTargets;

    qDebug() << "[DEBUG-4 Extractor] scene.specificTargets copiado, size="
             << scene.specificTargets.size();
    for (const auto& t : scene.specificTargets)
        qDebug() << "   ->" << t.id << "font=" << t.font.family() << t.font.pointSize()
                  << "bold=" << t.bold;

    // ----------------------------------------------------------
// Viewport lógico (normalización explícita)
// ----------------------------------------------------------

scene.viewportSize = QSizeF(
    core->getProjection(StelCore::FrameJ2000)->getViewportWidth(),
    core->getProjection(StelCore::FrameJ2000)->getViewportHeight()
);

  //----------------------------------------------------------
    // Capturar estado visual
    //----------------------------------------------------------

    captureModuleState(scene);

    scene.state.horizonLineVisible = options.showHorizon;
    scene.state.grayscale          = options.grayscale;
    scene.state.planetScale        = options.planetScale;

    //----------------------------------------------------------
    // Capturar primitivas gráficas
    //----------------------------------------------------------

    captureStars(scene);

    captureSolarSystem(scene);

    captureNebulae(scene);

    captureConstellations(scene);

    captureGrid(scene);

    captureMilkyWay(scene);

    captureHorizon(scene);

    captureLabels(scene);

    //----------------------------------------------------------
    // Post-procesamiento maestro de perfiles de color
    //----------------------------------------------------------
    if (options.colorMode == "Escala de grises" || options.colorMode == "Monocromo")
    {
        const bool isMono = (options.colorMode == "Monocromo");

        for (auto& star : scene.stars) {
            QColor c = star.pen.color();
            if (isMono) c = Qt::black;
            else { int g = qGray(c.rgb()); c = QColor(g, g, g); }
            star.pen.setColor(c);
            star.brush.setColor(c);
        }

        for (auto& planet : scene.planets) {
            if (isMono) planet.color = Qt::black;
            else { int g = qGray(planet.color.rgb()); planet.color = QColor(g, g, g); }
        }

        for (auto& neb : scene.nebulae) {
            if (isMono) neb.color = Qt::black;
            else { int g = qGray(neb.color.rgb()); neb.color = QColor(g, g, g); }
        }

        for (auto& line : scene.polylines) {
            if (isMono) line.color = Qt::black;
            else { int g = qGray(line.color.rgb()); line.color = QColor(g, g, g); }
        }
    }

    //----------------------------------------------------------
    // Actualizar estadísticas
    //----------------------------------------------------------

    scene.updateStatistics();

    qDebug()
    << "SkySceneExtractor:"
    << scene.primitiveCount()
    << "graphical primitives exported.";

if (scene.stars.size() > 1)
{
    
  std::stable_sort(scene.stars.begin(), scene.stars.end(),
    [](const SkyPoint& a, const SkyPoint& b)
    {
        return a.magnitude < b.magnitude;
    });
}

qDebug() << "SkySceneExtractor: scene.texts.size() =" << scene.texts.size()
         << "| polylines =" << scene.polylines.size();

    return true;
}

//*********************************************************************
// Seccion II: Captura el estado visual actual de Stellarium.
//
// Esta función únicamente almacena el estado de visibilidad de los
// distintos módulos. No genera geometría ni primitivas gráficas.
//*********************************************************************

void SkySceneExtractor::captureModuleState(SkyScene& scene)
{
    //----------------------------------------------------------
    // Valores por defecto
    //----------------------------------------------------------

    scene.state.starsVisible                    = false;
    scene.state.planetsVisible                  = false;
    scene.state.nebulaeVisible                  = false;

    scene.state.constellationLinesVisible       = false;
    scene.state.constellationLabelsVisible      = false;
    scene.state.constellationBoundariesVisible  = false;

    scene.state.equatorialGridVisible           = false;
    scene.state.horizontalGridVisible           = false;

    scene.state.equatorVisible                  = false;
    scene.state.eclipticVisible                 = false;
    scene.state.meridianVisible                 = false;

    
    
    scene.state.atmosphereVisible               = false;
    

    scene.state.cardinalPointsVisible           = false;

    scene.state.starLabelsVisible               = false;
    scene.state.planetLabelsVisible             = false;
    scene.state.deepSkyLabelsVisible            = false;
    
    scene.state.coordinateSystem =
            SkyCoordinateSystem::None;

    //----------------------------------------------------------
    // StarMgr
    //----------------------------------------------------------

    if (m_starMgr)
    {
        scene.state.starsVisible =
                m_starMgr->getFlagStars();

        scene.state.starLabelsVisible =
                m_starMgr->getFlagLabels();
    }

    //----------------------------------------------------------
    // ConstellationMgr
    //----------------------------------------------------------

    if (m_constMgr)
    {
        scene.state.constellationLinesVisible =
                m_constMgr->getFlagLines();

        scene.state.constellationLabelsVisible =
                m_constMgr->getFlagLabels();

        scene.state.constellationBoundariesVisible =
                m_constMgr->getFlagBoundaries();
    }

//----------------------------------------------------------
// NebulaMgr (Conectado al botón de la interfaz de Stellarium)
//----------------------------------------------------------
if (m_nebulaMgr)
{
    // Consulta directa al botón de la barra de herramientas (DSO/Nebulas)
    scene.state.nebulaeVisible = m_nebulaMgr->getFlagHints();

    scene.state.deepSkyLabelsVisible = StelApp::getInstance().getSettings()->value("nebula/flag_labels", true).toBool();
}

    //----------------------------------------------------------
    // SolarSystem
    //----------------------------------------------------------

    if (m_solarSystem)
    {
        scene.state.planetsVisible =
                m_solarSystem->getFlagPlanets();

        scene.state.planetLabelsVisible =
                m_solarSystem->getFlagLabels();
    }

    
    //----------------------------------------------------------
// GridLinesMgr (Reemplazo moderno)
//----------------------------------------------------------
if (m_gridMgr)
{
    // Obtenemos los estados desde el settings manager
    auto* settings = StelApp::getInstance().getSettings();
    scene.state.horizontalGridVisible = m_gridMgr->getFlagAzimuthalGrid();
   scene.state.equatorialGridVisible = m_gridMgr->getFlagEquatorGrid();
   scene.state.equatorVisible        = m_gridMgr->getFlagEquatorLine();
   scene.state.eclipticVisible       = m_gridMgr->getFlagEclipticLine();
   scene.state.meridianVisible       = m_gridMgr->getFlagMeridianLine();
   scene.state.horizonLineVisible    = m_gridMgr->getFlagHorizonLine(); 
   scene.state.eclipticJ2000Visible  = m_gridMgr->getFlagEclipticJ2000Line(); // J2000
}

    //----------------------------------------------------------
    // Sistema de coordenadas
    //----------------------------------------------------------

    if (scene.state.equatorialGridVisible)
    {
        scene.state.coordinateSystem =
                SkyCoordinateSystem::Equatorial;
    }
    else if (scene.state.horizontalGridVisible)
    {
        scene.state.coordinateSystem =
                SkyCoordinateSystem::Horizontal;
    }
    else
    {
        // Ninguna cuadrícula está activa: estado neutro real.
        // (Antes esto caía en Horizontal por error, y por eso el rótulo
        // "Cuadrícula Azimutal" aparecía aunque ninguna cuadrícula
        // estuviera encendida.)
        scene.state.coordinateSystem = SkyCoordinateSystem::None;
    }

   //----------------------------------------------------------
   // FLECHA NORTE (Sentido horario corregido, manteniendo Norte/Sur intactos)
   //----------------------------------------------------------
   
   if (m_core) {
    StelProjectorP projector = m_core->getProjection(StelCore::FrameAltAz);
    if (projector) {
        Vec3d northAltAz;
        // Azimut nativo de Stellarium = 180° (M_PI) equivale al Norte en la
        // convención Norte=0°, Este=90°, Sur=180°, Oeste=270°. NO tocar esto.
        StelUtils::spheToRect(M_PI, 0.0, northAltAz);

        Vec3d screenPos;
        if (projector->project(northAltAz, screenPos)) {
            QPointF center(scene.viewportSize.width() / 2.0, scene.viewportSize.height() / 2.0);
            QPointF northPt(screenPos[0], screenPos[1]);
            QPointF dir = northPt - center;

            // atan2(y, x) en coordenadas de pantalla (Y hacia abajo) ya crece en
            // sentido horario visual, así que NO se niega. El offset correcto es
            // +90 (no -90): con Norte arriba del centro, dir=(0,-1) => atan2=-90°,
            // y sumando 90 da 0° (sin rotación). Con -90 se obtiene -180°, que es
            // el desfase de 180° que hacía apuntar la flecha al Sur.
            scene.state.northRotationAngle = qRadiansToDegrees(std::atan2(dir.y(), dir.x())) + 90.0;
        }
    }
}


} // <--- Esta llave es CRUCIAL porque cierra la función captureModuleState completa.

//*********************************************************************
// SECCIÓN III
// captureStars()
//
// Captura todas las estrellas visibles y las convierte en
// primitivas SkyPoint.
//
// El extractor únicamente recopila información.
// No realiza ningún dibujo.
//*********************************************************************

void SkySceneExtractor::captureStars(SkyScene& scene)
{
    if (!m_starMgr || !m_core) return;
    if (!scene.state.starsVisible) return;

    const QList<StelObjectP>& stars = m_starMgr->getHipparcosStars();
    if (stars.isEmpty()) return;

    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    // Precálculos: evitamos leer viewportSize en cada iteración
    const double vpW = scene.viewportSize.width();
    const double vpH = scene.viewportSize.height();
    if (vpW <= 0.0 || vpH <= 0.0) return;

    // Reserva conservadora para rendimiento
    scene.stars.reserve(scene.stars.size() + qMin(stars.size(), 8000));

    for (const StelObjectP& object : stars)
    {
        // En Stellarium la magnitud visual se obtiene pasando m_core
        const double magnitude = object->getVMagnitude(m_core);

        // Comprobar si la estrella está en la lista de la sección Específica
        bool isSpecific = false;
        SpecificAstroRule specificRule;

      for (const auto& target : scene.specificTargets)
        {
            if (target.id == object->getID() || object->getID().startsWith(target.id + " ") || target.displayName == object->getNameI18n())
{
    isSpecific = true;
    specificRule = target;
    qDebug() << "[DEBUG-5] MATCH real object->getID()=" << object->getID() << "vs target.id=" << target.id;
    break;
}
        }
        if (!isSpecific && !scene.specificTargets.isEmpty())
        {
            // Solo para las primeras estrellas, evita saturar el log
            static int noMatchLogged = 0;
            if (noMatchLogged < 5)
            {
                qDebug() << "[DEBUG-5b Extractor] sin match, ejemplo real object->getID()=" << object->getID();
                ++noMatchLogged;
            }
        }

        // Si no es específica y supera la magnitud general, se descarta.
        // Si ES específica, se evalúa con la magnitud propia definida en la sección específica.
        if (!isSpecific && magnitude > scene.view.limitingMagnitude)
            continue;

        if (isSpecific && magnitude > specificRule.limitingMagnitude)
            continue;

        Vec3d position = object->getJ2000EquatorialPos(m_core);
        Vec3d screen;

        if (scene.state.horizonLineVisible && !isAboveHorizon(position))
            continue;

        if (!projector->project(position, screen))
            continue;

        const double nx = (screen[0] / vpW) * 2.0 - 1.0;
        const double ny = (screen[1] / vpH) * 2.0 - 1.0;

        SkyPoint star;
        star.label = resolveStarLabel(object);
        star.id = object->getID();
        star.position = QPointF(nx, ny);
        star.magnitude = magnitude;

        // Color según índice B-V
        float bv = 0.65f; // Fallback neutro
        if (magnitude <= 4.5f)
        {
            const QVariantMap info = object->getInfoMap(m_core);
            if (info.contains("bV"))
                bv = info.value("bV").toFloat();
        }

        QColor c = SkyChartRenderer::starColorForBV(bv);

        star.pen.setColor(c);
        star.pen.setWidthF(0.0);
        star.brush.setColor(c);
        star.brush.setStyle(Qt::SolidPattern);

        scene.stars.push_back(star);
    }

    scene.statistics.starCount = scene.stars.size();
}

//*********************************************************************
// Seccion IV: Captura todos los objetos del Sistema Solar visibles.
//
// Convierte cada planeta visible en un SkySymbol.
// El renderizador será el encargado de dibujarlo.
//*********************************************************************

void SkySceneExtractor::captureSolarSystem(SkyScene& scene)
{
    if (!m_solarSystem || !m_core)
        return;

    //----------------------------------------------------------
    // Si el usuario ocultó los planetas
    //----------------------------------------------------------

    if (!scene.state.planetsVisible)
        return;

    //----------------------------------------------------------
    // Proyector actual
    //----------------------------------------------------------

    StelProjectorP projector =
        m_core->getProjection(StelCore::FrameJ2000);

    if (!projector)
        return;

    //----------------------------------------------------------
    // Obtener todos los planetas
    //----------------------------------------------------------

    const QStringList names = m_solarSystem->getAllPlanetEnglishNames();

    qDebug() << "SkySceneExtractor: planetsVisible =" << scene.state.planetsVisible
             << "| nombres obtenidos =" << names.size();

    scene.planets.reserve(
        scene.planets.size() + names.size());

    int exported = 0;

    //----------------------------------------------------------
    // Recorrer el Sistema Solar
    //----------------------------------------------------------

    for (const QString& englishName : names)
    {
        static const QSet<QString> visibleBodies = {
            "Sun", "Mercury", "Venus", "Moon", "Mars",
            "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"
        };

        // Excluye satélites artificiales, asteroides, cometas y a la propia
        // Tierra (no tiene sentido dibujarla, la observamos desde ella).
        if (!visibleBodies.contains(englishName))
            continue;

        PlanetP planet = m_solarSystem->searchByEnglishName(englishName);
        if (!planet)
            continue;

        //------------------------------------------------------
        // Posición J2000
        //------------------------------------------------------

        Vec3d position =
            planet->getJ2000EquatorialPos(m_core);

        Vec3d screen;

        if (scene.state.horizonLineVisible && !isAboveHorizon(position))
            continue;

        if (!projector->project(position, screen))
            continue;

        qDebug() << "  ->" << englishName << "proyectado OK, mag =" << planet->getVMagnitude(m_core);
        //------------------------------------------------------
        // Coordenadas normalizadas [-1,+1]
        //------------------------------------------------------

        const double nx =
            (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0;

        const double ny =
            (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0;

        //------------------------------------------------------
        // Magnitud del planeta y Crear símbolo
        //------------------------------------------------------

        float magnitude = static_cast<float>(planet->getVMagnitude(m_core));

        QString planetNameI18n = planet->getNameI18n();
        if (planetNameI18n.isEmpty())
            planetNameI18n = planet->getEnglishName();

        // ¿Este planeta tiene una regla de la sección Específica asociada?
        // Mismo criterio de coincidencia que ya usa captureStars(): por id
        // "crudo" del objeto, por nombre inglés interno, o por displayName.
        bool isSpecificPlanet = false;
        SpecificAstroRule specificPlanetRule;
        for (const auto& target : scene.specificTargets)
        {
            if (target.id == planet->getID() ||
                target.id == englishName ||
                target.displayName == planetNameI18n)
            {
                isSpecificPlanet = true;
                specificPlanetRule = target;
                break;
            }
        }

        // Filtro: sin esto se capturan ~700 objetos invisibles a simple vista.
        // Igual que con las estrellas: un planeta específico usa SU PROPIA
        // magnitud límite, no la General.
        if (!isSpecificPlanet && magnitude > scene.view.limitingMagnitude)
            continue;
        if (isSpecificPlanet && magnitude > specificPlanetRule.limitingMagnitude)
            continue;

        SkySymbol symbol;
        symbol.type     = SkySymbolType::Planet;
        symbol.name     = planetNameI18n;
        symbol.position = QPointF(nx, ny);

        // ------------------------------------------------------------------
        // EXTRACCIÓN Y NORMALIZACIÓN ARQUEOASTRONÓMICA (CORREGIDA):
        //
        // 1) Se consulta el radio angular real (planet->getAngularRadius).
        // 2) Si el cuerpo es Sol/Luna, Stellarium amplifica su disco
        //    internamente mediante sphereScale para que el usuario aprecie
        //    fases y textura. Se neutraliza dividiendo por el factor de
        //    amplificación VIGENTE en este instante (getSphereScale()),
        //    NUNCA por una constante fija: ese factor puede cambiar según
        //    la configuración activa de Stellarium.
        // 3) Se aplica el factor de escala de disco configurado por el
        //    usuario en el diálogo ("Escala"), UNA sola vez, aquí. Antes se
        //    calculaba y se descartaba -> por eso el slider no tenía efecto
        //    sobre planetas/Sol/Luna.
        // ------------------------------------------------------------------
      

double angularRadius = 0.0;
if (planet)
{
    // getAngularRadius() devuelve GRADOS en este build (confirmado por log:
    // Moon -> 0.263283, que coincide con el radio angular real ~0.26°).
    // Se convierte a radianes ANTES de multiplicar por pixelPerRad.
    angularRadius = planet->getAngularRadius(m_core) * DEG2RAD;
}

double pixelPerRad = projector->getPixelPerRadAtCenter();
double userDiscScale = qMax(0.01, scene.state.planetScale);

// Tamaño por convención de brillo: usa bodyRadiusForMagnitude (no
// starRadiusForMagnitude), porque el Sol/Luna/planetas brillantes caen
// muy por debajo del piso de magnitud estelar (-1.5, Sirio) y necesitan
// la rama de crecimiento adicional que YA existe en el renderer y que
// las nebulosas sí estaban usando correctamente.
double magnitudeRadiusPx = SkyChartRenderer::bodyRadiusForMagnitude(
                               magnitude, scene.view.limitingMagnitude, userDiscScale);

// Tamaño físico real (ya corregido en grados->radianes): domina solo
// cuando el campo de visión es tan angosto que el disco real es
// visualmente más grande que el símbolo de magnitud (ej. primer plano
// de la Luna o Saturno).
double physicalRadiusPx = angularRadius * pixelPerRad * userDiscScale;

double baseRadiusPx = qMax(magnitudeRadiusPx, physicalRadiusPx);

if (baseRadiusPx < 1.0) {
    baseRadiusPx = 1.0;
}

symbol.scale = baseRadiusPx;

       if (englishName == "Moon")
{
    const QVariantMap moonInfo = planet->getInfoMap(m_core);
    const double illuminationPct = moonInfo.value("illumination", 100.0).toDouble();
    symbol.rotation = qBound(0.0, illuminationPct / 100.0, 1.0);
    symbol.waning = moonInfo.value("is-waning", false).toBool(); // se deja para debug, ya no se usa para el dibujo

    // Dirección real hacia el Sol, con el MISMO projector ya usado arriba
    // para todos los demás cuerpos: hereda automáticamente rotación de
    // norte, modo Az/Alt vs RA/Dec, todo.
    if (PlanetP sunPlanet = m_solarSystem->searchByEnglishName("Sun"))
    {
        Vec3d sunScreen;
        if (projector->project(sunPlanet->getJ2000EquatorialPos(m_core), sunScreen))
        {
            symbol.sunDirection = QPointF(
                (sunScreen[0] / scene.viewportSize.width())  * 2.0 - 1.0,
                (sunScreen[1] / scene.viewportSize.height()) * 2.0 - 1.0);
            symbol.hasSunDirection = true;
        }
    }
}
        else
        {
            symbol.rotation = 1.0;
            symbol.waning = false;
        }

        QColor baseColor;
        if (englishName == "Sun")          baseColor = QColor(255, 215, 60);
        else if (englishName == "Mercury") baseColor = QColor(180, 180, 180);
        else if (englishName == "Venus")   baseColor = QColor(255, 250, 220);
        else if (englishName == "Moon")    baseColor = QColor(225, 225, 225);
        else if (englishName == "Mars")    baseColor = QColor(226, 110, 80);
        else if (englishName == "Jupiter") baseColor = QColor(230, 200, 150);
        else if (englishName == "Saturn")  baseColor = QColor(230, 210, 160);
        else if (englishName == "Uranus")  baseColor = QColor(160, 220, 230);
        else if (englishName == "Neptune") baseColor = QColor(120, 150, 230);
        else                               baseColor = QColor(255, 240, 180);

        symbol.color = baseColor;


        symbol.layer    = SkyLayer::SolarSystem;

        scene.planets.append(symbol);

        // Etiqueta con el nombre. Si el planeta está en la lista de
        // Astros Específicos, se usa SU fuente/negrita/cursiva; si no,
        // se deja el QFont() por defecto (comportamiento general, sin cambios).
        SkyText label;
        label.position = symbol.position;
        label.text     = symbol.name;
        if (isSpecificPlanet)
        {
            label.font = specificPlanetRule.font;
            label.color = Qt::black;
        }
        scene.texts.push_back(label);

        ++exported;
    }

    //----------------------------------------------------------
    // Estadísticas
    //----------------------------------------------------------

    scene.statistics.planetCount =
        scene.planets.size();

    qDebug()
        << "SkySceneExtractor:"
        << exported
        << "solar system objects captured.";
}


//*********************************************************************
// Seccion V: Captura todos los objetos de cielo profundo visibles.
//
// Convierte cada nebulosa visible en un SkySymbol para que el
// renderizador pueda representarla posteriormente.
//
// No dibuja.
//*********************************************************************

void SkySceneExtractor::captureNebulae(SkyScene& scene)
{
    if (!m_nebulaMgr || !m_core) return;
    if (!scene.state.nebulaeVisible) return;

    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    QList<std::pair<QString, StelObjectP>> dsoList = m_nebulaMgr->listAllObjects(false);
    if (dsoList.isEmpty()) return;

    scene.nebulae.reserve(scene.nebulae.size() + dsoList.size());

    int skipped = 0;
    for (const auto& pair : dsoList)
    {
        const StelObjectP& obj = pair.second;
        if (!obj) continue;

        const float mag = static_cast<float>(obj->getVMagnitude(m_core));
        if (mag > scene.view.limitingMagnitude)
        {
            ++skipped;
            continue;
        }

        Vec3d position = obj->getJ2000EquatorialPos(m_core);
        Vec3d screenPos;

        if (scene.state.horizonLineVisible && !isAboveHorizon(position))
            continue;

        if (!projector->project(position, screenPos)) continue;

        const double nx = (screenPos[0] / scene.viewportSize.width()) * 2.0 - 1.0;
        const double ny = (screenPos[1] / scene.viewportSize.height()) * 2.0 - 1.0;

        SkySymbol symbol;
        symbol.position = QPointF(nx, ny);
        symbol.name     = obj->getNameI18n();
        if (symbol.name.isEmpty())
            symbol.name = obj->getEnglishName();

        SkySymbolType dsoType = SkySymbolType::DiffuseNebula;
        QColor baseColor = QColor(120, 200, 255);

        if (const Nebula* neb = dynamic_cast<const Nebula*>(obj.data()))
        {
           switch (neb->getDSOType())
            {
                case Nebula::NebGx: case Nebula::NebAGx: case Nebula::NebRGx:
                case Nebula::NebIGx: case Nebula::NebQSO: case Nebula::NebBLL:
                case Nebula::NebBLA: case Nebula::NebGxCl: case Nebula::NebPartOfGx:
                    dsoType = SkySymbolType::Galaxy;
                    baseColor = QColor(190, 170, 220);
                    break;
                case Nebula::NebCl: case Nebula::NebOc: case Nebula::NebSA: case Nebula::NebSC:
                    dsoType = SkySymbolType::OpenCluster;
                    baseColor = QColor(255, 215, 140);
                    break;
                case Nebula::NebGc:
                    dsoType = SkySymbolType::GlobularCluster;
                    baseColor = QColor(255, 190, 110);
                    break;
                case Nebula::NebPn: case Nebula::NebPossPN: case Nebula::NebPPN:
                    dsoType = SkySymbolType::PlanetaryNebula;
                    baseColor = QColor(140, 220, 200);
                    break;
                case Nebula::NebDn:
                    dsoType = SkySymbolType::DarkNebula;
                    baseColor = QColor(120, 120, 120);
                    break;
                case Nebula::NebEn: case Nebula::NebHII: case Nebula::NebSNR: case Nebula::NebEMO:
                    dsoType = SkySymbolType::EmissionNebula;
                    baseColor = QColor(255, 130, 130);
                    break;
                case Nebula::NebRegion:
                    dsoType = SkySymbolType::SkyRegion;
                    baseColor = QColor(150, 150, 150);
                    break;
                default:
                    dsoType = SkySymbolType::DiffuseNebula;
                    baseColor = QColor(120, 200, 255);
                    break;
            }
        }

        symbol.color    = baseColor;
        symbol.type     = dsoType;
        symbol.layer    = SkyLayer::Nebulae;

        double pixelPerRad = projector->getPixelPerRadAtCenter();
        double userDiscScale = qMax(0.01, scene.state.planetScale);
        
        double magnitudeRadiusPx = SkyChartRenderer::bodyRadiusForMagnitude(
                                       mag, scene.view.limitingMagnitude, userDiscScale);
                                       
        QVariantMap infoMap = obj->getInfoMap(m_core);
        double axisMajor = infoMap.value("axis-major-dd", 0.0).toDouble();
        double axisMinor = infoMap.value("axis-minor-dd", 0.0).toDouble();
        double orientAngle = infoMap.value("orientation-angle", 0.0).toDouble();

        if (axisMajor > 0.0)
        {
            double radMajor = (axisMajor / 2.0) * DEG2RAD;
            double radMinor = (axisMinor > 0.0 ? axisMinor / 2.0 : axisMajor / 2.0) * DEG2RAD;

            symbol.scale = radMajor * pixelPerRad * userDiscScale;
            symbol.scaleY = radMinor * pixelPerRad * userDiscScale;
            symbol.rotation = orientAngle; 
            
            if (symbol.scale < magnitudeRadiusPx) symbol.scale = magnitudeRadiusPx;
            if (symbol.scaleY < magnitudeRadiusPx) symbol.scaleY = magnitudeRadiusPx;
        }
        else
        {
            double angularRadius = obj->getAngularRadius(m_core) * DEG2RAD;
            double physicalRadiusPx = angularRadius * pixelPerRad * userDiscScale;
            double baseRadiusPx = qMax(magnitudeRadiusPx, physicalRadiusPx);
            if (baseRadiusPx < 2.0) baseRadiusPx = 2.0;

            symbol.scale = baseRadiusPx;
            symbol.scaleY = baseRadiusPx;
            symbol.rotation = 0.0;
        }

        scene.nebulae.append(symbol);
    }

    scene.statistics.nebulaCount = scene.nebulae.size();
    qDebug() << "SkySceneExtractor:" << scene.nebulae.size()
              << "nebulae captured," << skipped << "skipped by magnitude.";
}

//*********************************************************************
// Seccion VI: Captura el estado de las constelaciones.
//
// En el nuevo modelo SkyScene no existen objetos SkyConstellation.
// Esta rutina únicamente registra qué elementos de las constelaciones
// están visibles.
//
// La geometría (líneas y etiquetas) será añadida posteriormente
// mediante SkyPolyline y SkyText.
//*********************************************************************

void SkySceneExtractor::captureConstellations(SkyScene& scene)
{
    if (!m_constMgr || !m_core || !m_starMgr)
        return;

    if (!scene.state.constellationLinesVisible &&
        !scene.state.constellationLabelsVisible &&
        !scene.state.constellationBoundariesVisible)
    {
        return;
    }

    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    // --- Nombres (API pública de ConstellationMgr) ---
    if (scene.state.constellationLabelsVisible)
    {
        QVector<QPair<QString, StelObjectP>> constellations = m_constMgr->listAllObjects(false);
        for (const auto& pair : constellations)
        {
            const StelObjectP& obj = pair.second;
            if (!obj) continue;

            Vec3d position = obj->getJ2000EquatorialPos(m_core);

if (scene.state.horizonLineVisible && !isAboveHorizon(position))
    continue;

Vec3d screen;
if (!projector->project(position, screen)) continue;

            SkyText label;
            label.position = QPointF(
                (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0);
            label.text = obj->getNameI18n();
            if (label.text.isEmpty())
                label.text = obj->getEnglishName();

            scene.texts.push_back(label);
        }
    }

    // --- Líneas reales: leídas directamente del index.json (no expuestas por ConstellationMgr) ---
    if (scene.state.constellationLinesVisible)
    {
        buildConstellationLines(scene);
    }
}

//*********************************************************************
// Seccion VII: Captura el estado de las cuadrículas.
//
// En el nuevo modelo SkyScene la geometría de las cuadrículas no se
// extrae desde Stellarium. El extractor únicamente registra qué
// elementos están visibles.
//
// La construcción de las polilíneas será realizada posteriormente por
// el plugin utilizando SkyPolyline.
//*********************************************************************

void SkySceneExtractor::captureGrid(SkyScene& scene)
{
    if (!m_gridMgr || !m_core)
        return;

    if (!scene.state.horizontalGridVisible &&
        !scene.state.equatorialGridVisible &&
        !scene.state.equatorVisible &&
        !scene.state.eclipticVisible &&
        !scene.state.meridianVisible &&
        !scene.state.eclipticJ2000Visible&&
        !scene.state.cardinalPointsVisible)
    {
        return;
    }

    // Validar estrictamente la visibilidad de las cuadrículas
    if (!scene.state.horizontalGridVisible && !scene.state.equatorialGridVisible)
    {
        // Si no hay ninguna cuadrícula activa, nos aseguramos de que no se procese texto ni etiquetas de coordenadas
        return;
    }

    if (scene.state.horizontalGridVisible)
        scene.state.coordinateSystem = SkyCoordinateSystem::Horizontal;
    else if (scene.state.equatorialGridVisible)
        scene.state.coordinateSystem = SkyCoordinateSystem::Equatorial; 

    if (scene.state.equatorialGridVisible)
        buildEquatorialGridLines(scene);

    if (scene.state.eclipticVisible)
        buildEclipticLine(scene, true);  // de fecha
    
    if (scene.state.eclipticJ2000Visible)
       buildEclipticLine(scene, false); // J2000

    if (scene.state.horizontalGridVisible)
        buildAzimuthalGridLines(scene);

    qDebug() << "SkySceneExtractor:" << scene.polylines.size() << "grid polylines generated.";
}

//*********************************************************************

//*********************************************************************

void SkySceneExtractor::captureHorizon(SkyScene& scene)
{
    if (!m_gridMgr || !m_core)
        return;

    if (!scene.state.horizonLineVisible)
        return;

    buildHorizonLine(scene);
}

//*********************************************************************
// Nuevo método dedicado — replica exactamente el comportamiento de
// los dos checkboxes de Stellarium (nombres comunes / designaciones),
// pero de forma independiente para el plugin.
//*********************************************************************

namespace {
// StarMgr::getSciDesignation() a veces devuelve una designación compuesta
// cuando la estrella tiene tanto Bayer como Flamsteed, ej. "β Ori - 19 Ori".
// Para la etiqueta impresa en la carta solo queremos la primera parte
// ("β Ori"): la segunda es redundante (mismo objeto, otro catálogo) y
// satura visualmente el plano. Se corta en el primer " - " literal.
QString simplifyStarDesignation(const QString& raw)
{
    const int idx = raw.indexOf(QStringLiteral(" - "));
    if (idx > 0)
        return raw.left(idx).trimmed();
    return raw;
}
}

QString SkySceneExtractor::resolveStarLabel(const StelObjectP& object) const
{
    if (!object) return QString();

    // El ID real viene como "HIP 12345" (a veces con letra de componente,
    // ej. "HIP 116737 A"), NO como número puro -> toInt() directo siempre
    // fallaba (confirmado por log: ok=false en el 100% de los casos).
    // Se extrae el número con una expresión regular en vez de asumir
    // formato numérico simple.
    static const QRegularExpression hipRe(QStringLiteral("HIP\\s*(\\d+)"));
    const QRegularExpressionMatch match = hipRe.match(object->getID());
    const bool ok = match.hasMatch();
    const int hip = ok ? match.captured(1).toInt() : 0;

    if (ok && hip > 0)
    {
        // StarMgr::getDesignationUsage() es EXACTAMENTE el mismo flag
        // que enciende el checkbox de designaciones en la ventana de
        // Vista de Stellarium. No hay que replicar nada: solo leerlo.
        if (StarMgr::getDesignationUsage())
        {
            const QString catalog = simplifyStarDesignation(StarMgr::getSciDesignation(hip));
            if (!catalog.isEmpty())
                return catalog;
        }

        const QString common = StarMgr::getCommonNameI18n(hip);
        if (!common.isEmpty())
            return common;

        // Sin nombre común: Stellarium igual muestra la designación
        // científica si existe, para no dejar la estrella sin etiqueta.
        const QString catalog = simplifyStarDesignation(StarMgr::getSciDesignation(hip));
        if (!catalog.isEmpty())
            return catalog;
    }

    // Fallback (objetos sin HIP resoluble)
    QString label = object->getNameI18n();
    if (label.isEmpty())
        label = object->getEnglishName();
    return label;
}


//*********************************************************************
// Seccion XIII: Captura y valida las etiquetas.
//
// En el nuevo SkyScene todas las etiquetas se almacenan como SkyText.
// Esta rutina elimina las entradas vacías y actualiza las estadísticas.
//*********************************************************************

void SkySceneExtractor::captureLabels(SkyScene& scene)
{
    //----------------------------------------------------------
    // Eliminar etiquetas vacías
    //----------------------------------------------------------

    auto endIt = std::remove_if(
        scene.texts.begin(),
        scene.texts.end(),
        [](const SkyText& text)
        {
            return text.text.trimmed().isEmpty();
        });

    scene.texts.erase(endIt, scene.texts.end());

    //----------------------------------------------------------
    // Estadísticas
    //----------------------------------------------------------

    scene.updateStatistics();

    //----------------------------------------------------------

    qDebug()
        << "SkySceneExtractor:"
        << scene.texts.size()
        << "labels validated.";
}



//*********************************************************************


//*********************************************************************

namespace {

bool findFrameEdgeCrossing(const SkyPolyline& line, QPointF& outPoint)
{
    // Revisa los 4 bordes del marco en orden de preferencia:
    // arriba, izquierda, derecha, abajo. Devuelve el primer cruce real que encuentre.
    const double edges[4] = { -1.0, -1.0, 1.0, 1.0 };
    const bool isHorizontalEdge[4] = { true, false, false, true }; // true = borde en Y (arriba/abajo)

    for (int e = 0; e < 4; ++e)
    {
        for (int i = 0; i + 1 < line.points.size(); ++i)
        {
            const QPointF& a = line.points[i];
            const QPointF& b = line.points[i + 1];

            if (isHorizontalEdge[e])
            {
                const double edgeY = edges[e];
                if ((a.y() - edgeY) * (b.y() - edgeY) <= 0.0 && a.y() != b.y())
                {
                    const double t = (edgeY - a.y()) / (b.y() - a.y());
                    const double x = a.x() + t * (b.x() - a.x());
                    if (x >= -1.0 && x <= 1.0)
                    {
                        outPoint = QPointF(x, edgeY);
                        return true;
                    }
                }
            }
            else
            {
                const double edgeX = edges[e];
                if ((a.x() - edgeX) * (b.x() - edgeX) <= 0.0 && a.x() != b.x())
                {
                    const double t = (edgeX - a.x()) / (b.x() - a.x());
                    const double y = a.y() + t * (b.y() - a.y());
                    if (y >= -1.0 && y <= 1.0)
                    {
                        outPoint = QPointF(edgeX, y);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}


QPointF findClosestToFrameEdge(const SkyPolyline& line)
{
    QPointF best;
    double bestDist = std::numeric_limits<double>::max();
    for (const QPointF& p : line.points)
    {
        const double distToEdge = qMin(qMin(1.0 - p.x(), p.x() + 1.0),
                                        qMin(1.0 - p.y(), p.y() + 1.0));
        if (distToEdge < bestDist)
        {
            bestDist = distToEdge;
            best = p;
        }
    }
    return best;
}


} // namespace


//*********************************************************************
// Busca el punto exacto donde 'pts' cruza el borde del recuadro [-1,1]x[-1,1],
// interpolando entre las dos muestras consecutivas que quedan a ambos lados
// del borde. Devuelve false si el segmento nunca sale del recuadro.
//*********************************************************************


static bool findBoxEdgeCrossing(const QVector<QPointF>& pts, QPointF& outPoint)
{
    auto inBox = [](const QPointF& p) {
        return qAbs(p.x()) <= 1.0 && qAbs(p.y()) <= 1.0;
    };

    for (int i = 0; i + 1 < pts.size(); ++i)
    {
        const bool in0 = inBox(pts[i]);
        const bool in1 = inBox(pts[i + 1]);
        if (in0 == in1) continue; // ambos dentro o ambos fuera: no hay cruce acá

        const QPointF& a = in0 ? pts[i]     : pts[i + 1];
        const QPointF& b = in0 ? pts[i + 1] : pts[i];
        // a: dentro del recuadro, b: fuera. Interpolamos a -> b.

        double t = 1.0;
        if (b.x() > 1.0 || b.x() < -1.0)
        {
            const double edgeX = (b.x() > 1.0) ? 1.0 : -1.0;
            t = qMin(t, (edgeX - a.x()) / (b.x() - a.x()));
        }
        if (b.y() > 1.0 || b.y() < -1.0)
        {
            const double edgeY = (b.y() > 1.0) ? 1.0 : -1.0;
            t = qMin(t, (edgeY - a.y()) / (b.y() - a.y()));
        }

        outPoint = a + t * (b - a);
        return true;
    }
    return false;
}


//*********************************************************************
// Helper: genera la geometría real de la cuadrícula (ecuatorial u
// horizontal/azimutal) muestreando meridianos y paralelos y
// proyectándolos con el projector correspondiente al frame indicado.
// GridLinesMgr no expone sus puntos internos, así que la calculamos
// nosotros mismos con la misma técnica que ya usa captureStars().
//*********************************************************************

//*********************************************************************
// Cuadrícula ECUATORIAL (RA/Dec, frame J2000).
// Cálculo propio y separado del azimutal: aquí SÍ puede haber cruces
// de horizonte en mitad de una línea (una línea de RA constante sube
// y baja respecto del horizonte según la hora), por eso se interpola
// el cruce y se corta la polilínea ahí.
//*********************************************************************

void SkySceneExtractor::buildEquatorialGridLines(SkyScene& scene)
{
    if (!m_core) return;
    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    constexpr double DEG2RAD = 0.017453292519943295;

    // Definición dinámica adaptada a 5 minutos de tiempo (1.25°) para RA y 1° para Dec en acercamiento máximo
    double raStepDeg = 15.0;
    double decStepDeg = 15.0;
    if (auto* movementMgr = GETSTELMODULE(StelMovementMgr))
    {
        const double fov = movementMgr->getCurrentFov();
        if (fov > 60.0)      { raStepDeg = 15.0;  decStepDeg = 15.0; }
        else if (fov > 20.0) { raStepDeg = 5.0;   decStepDeg = 5.0;  }
        else if (fov > 5.0)  { raStepDeg = 1.25;  decStepDeg = 2.0;  } // 1.25° equivalen exactamente a 5 minutos de tiempo
        else                 { raStepDeg = 1.25;  decStepDeg = 1.0;  }
    }

    const double lonStepDeg = raStepDeg;
    const int lonDivisions = qRound(360.0 / lonStepDeg);

    const bool cutByHorizon = scene.state.horizonLineVisible;

    // ================= Meridianos (líneas de AR constante, en horas/minutos) =================
    for (int i = 0; i < lonDivisions; ++i)
    {
        const double lonDeg = i * lonStepDeg;
        const double lon = lonDeg * DEG2RAD;
        const bool isPrimeMeridian = (i == 0);

        // Texto del meridiano: se calcula UNA sola vez por línea, no por segmento.
        double totalHours = lonDeg / 15.0;
        int hours = int(floor(totalHours));
        int minutes = int(round((totalHours - hours) * 60.0));
        if (minutes >= 60) {
            hours = (hours + 1) % 24;
            minutes = 0;
        }
        const QString meridianLabel = (minutes == 0)
            ? QString("%1h").arg(hours)
            : QString("%1h %2m").arg(hours).arg(minutes, 2, 10, QChar('0'));

        // Segmentos de ESTA línea únicamente (puede fragmentarse por el horizonte).
        // El label se asigna al final, a UN solo segmento -> ver bloque tras el muestreo.
        QVector<SkyPolyline> segments;

        SkyPolyline line;
        Vec3d prevV;
        bool havePrev = false;
        bool prevAbove = true;
        int prevLatDeg = -90;
        bool segmentTruncatedByHorizon = false;

        auto flushSegment = [&]()
        {
            if (line.points.size() >= 2)
            {
                line.category           = 1;
                line.type               = SkyPolylineType::GridLine;
                line.width              = isPrimeMeridian ? 0.7f : 0.35f;
                line.color              = QColor(120, 120, 120);
                line.style              = Qt::SolidLine;
                line.isMeridianLine     = true;
                line.truncatedByHorizon = segmentTruncatedByHorizon;
                // labelText queda vacío aquí a propósito.
                segments.push_back(line);
            }
            line = SkyPolyline();
            segmentTruncatedByHorizon = false;
        };

        // Muestreo más denso (cada 1 grado) para evitar vacíos al acercar
        for (int latDeg = -85; latDeg <= 85; latDeg += 1)
        {
            Vec3d v;
            StelUtils::spheToRect(lon, latDeg * DEG2RAD, v);

            bool above = cutByHorizon ? isAboveHorizon(v) : true;

            if (havePrev && above != prevAbove)
            {
                const Vec3d prevAltAz = m_core->j2000ToAltAz(prevV, StelCore::RefractionOff);
                const Vec3d currAltAz = m_core->j2000ToAltAz(v, StelCore::RefractionOff);
                const double z0 = prevAltAz[2];
                const double z1 = currAltAz[2];
                if (z1 != z0)
                {
                    const double t = z0 / (z0 - z1);
                    const double crossLatDeg = prevLatDeg + t * (latDeg - prevLatDeg);
                    Vec3d crossV;
                    StelUtils::spheToRect(lon, crossLatDeg * DEG2RAD, crossV);
                    Vec3d crossScreen;
                    if (projector->project(crossV, crossScreen))
                    {
                        line.points.append(QPointF(
                            (crossScreen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                            (crossScreen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
                    }
                }

                segmentTruncatedByHorizon = true;
                flushSegment();
            }

            if (above)
            {
                Vec3d screen;
                if (projector->project(v, screen))
                {
                    line.points.append(QPointF(
                        (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                        (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
                }
            }

            prevV = v;
            prevLatDeg = latDeg;
            prevAbove = above;
            havePrev = true;
        }

        flushSegment();

        // -------- Un solo portador de etiqueta por línea: el segmento más largo --------
        if (!segments.isEmpty())
        {
            int bestIdx = 0;
            int bestPoints = segments[0].points.size();
            for (int s = 1; s < segments.size(); ++s)
            {
                if (segments[s].points.size() > bestPoints)
                {
                    bestPoints = segments[s].points.size();
                    bestIdx = s;
                }
            }
            segments[bestIdx].labelText = meridianLabel;
        }

        for (const SkyPolyline& seg : segments)
            scene.polylines.push_back(seg);
    }

    // ================= Paralelos (líneas de Dec constante, en grados) =================
    const double latStepDeg = decStepDeg;
    for (double latDegD = -75.0; latDegD <= 75.0; latDegD += latStepDeg)
    {
        const int latDeg = int(latDegD);
        const double lat = latDeg * DEG2RAD;
        const bool isCelestialEquator = (latDeg == 0);

        // Texto del paralelo: se calcula UNA sola vez por línea, no por segmento.
        const QString parallelLabel = QString("%1%2°").arg(latDeg >= 0 ? "+" : "").arg(latDeg);

        QVector<SkyPolyline> segments;

        SkyPolyline line;
        Vec3d prevV;
        bool havePrev = false;
        bool prevAbove = true;
        int prevLonDeg = -1;
        bool segmentTruncatedByHorizon = false;

        auto flushSegment = [&]()
        {
            if (line.points.size() >= 2)
            {
                line.category           = 1;
                line.type               = SkyPolylineType::GridLine;
                line.width              = isCelestialEquator ? 0.7f : 0.35f;
                line.color              = QColor(120, 120, 120);
                line.style              = Qt::SolidLine;
                line.isMeridianLine     = false;
                line.truncatedByHorizon = segmentTruncatedByHorizon;
                // labelText queda vacío aquí a propósito.
                segments.push_back(line);
            }
            line = SkyPolyline();
            segmentTruncatedByHorizon = false;
        };

        // Muestreo horizontal continuo cada 1 grado para evitar discontinuidades
        for (int lonDeg = 0; lonDeg <= 360; lonDeg += 1)
        {
            Vec3d v;
            StelUtils::spheToRect(lonDeg * DEG2RAD, lat, v);

            bool above = cutByHorizon ? isAboveHorizon(v) : true;

            if (havePrev && above != prevAbove)
            {
                const Vec3d prevAltAz = m_core->j2000ToAltAz(prevV, StelCore::RefractionOff);
                const Vec3d currAltAz = m_core->j2000ToAltAz(v, StelCore::RefractionOff);
                const double z0 = prevAltAz[2];
                const double z1 = currAltAz[2];
                if (z1 != z0)
                {
                    const double t = z0 / (z0 - z1);
                    const double crossLonDeg = prevLonDeg + t * (lonDeg - prevLonDeg);
                    Vec3d crossV;
                    StelUtils::spheToRect(crossLonDeg * DEG2RAD, lat, crossV);
                    Vec3d crossScreen;
                    if (projector->project(crossV, crossScreen))
                    {
                        line.points.append(QPointF(
                            (crossScreen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                            (crossScreen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
                    }
                }

                segmentTruncatedByHorizon = true;
                flushSegment();
            }

            if (above)
            {
                Vec3d screen;
                if (projector->project(v, screen))
                {
                    line.points.append(QPointF(
                        (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                        (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
                }
            }

            prevV = v;
            prevLonDeg = lonDeg;
            prevAbove = above;
            havePrev = true;
        }

        flushSegment();

        // -------- Un solo portador de etiqueta por línea: el segmento más largo --------
        if (!segments.isEmpty())
        {
            int bestIdx = 0;
            int bestPoints = segments[0].points.size();
            for (int s = 1; s < segments.size(); ++s)
            {
                if (segments[s].points.size() > bestPoints)
                {
                    bestPoints = segments[s].points.size();
                    bestIdx = s;
                }
            }
            segments[bestIdx].labelText = parallelLabel;
        }

        for (const SkyPolyline& seg : segments)
            scene.polylines.push_back(seg);
    }
}


//*********************************************************************
// Cuadrícula AZIMUTAL (Az/Alt, frame AltAz).
// Cálculo propio y separado del ecuatorial: aquí NUNCA hay que
// interpolar un cruce de horizonte, porque el horizonte es alt=0 por
// definición en este frame — simplemente no se muestrea por debajo.
// Por eso, cuando el horizonte está activo, toda línea que no llegue
// al marco queda truncada exactamente EN el horizonte (alt=0), sin
// aproximaciones.
//*********************************************************************

void SkySceneExtractor::buildAzimuthalGridLines(SkyScene& scene)
{
    if (!m_core) return;
    StelProjectorP projector = m_core->getProjection(StelCore::FrameAltAz);
    if (!projector) return;

    constexpr double DEG2RAD = 0.017453292519943295;
    // Azimutales a 1° dinámico según el campo de visión (FOV)
    double stepDeg = 15.0;
    if (auto* movementMgr = GETSTELMODULE(StelMovementMgr))
    {
        const double fov = movementMgr->getCurrentFov();
        if (fov > 60.0)      stepDeg = 15.0;
        else if (fov > 20.0) stepDeg = 5.0;
        else if (fov > 5.0)  stepDeg = 2.0;
        else                 stepDeg = 1.0; // 1° dinámico en acercamiento
    }

    const int lonDivisions = qMax(4, int(360.0 / stepDeg));
    const double lonStepDeg = 360.0 / lonDivisions;
    const bool cutByHorizon = scene.state.horizonLineVisible;
    const int minLatDeg = cutByHorizon ? 0 : -85;

    // ================= Meridianos de azimut constante (en grados) =================
    for (int i = 0; i < lonDivisions; ++i)
    {
        const double lonDeg = i * lonStepDeg;
        // Desfase de 180 grados para que el 0 esté en el Norte y no en el Sur nativo de
        // Stellarium, e inversión de signo para que el rótulo crezca en sentido horario
        // (Norte=0°, Este=90°, Sur=180°, Oeste=270°) en vez de antihorario. El offset por
        // sí solo (lonDeg + 180) NO cambia el sentido de giro, solo el punto de partida:
        // por eso el norte quedaba en 0 pero seguía leyendo antihorario.
        const double adjustedLonDeg = fmod(180.0 - lonDeg + 360.0, 360.0);
        const double lon = adjustedLonDeg * DEG2RAD;

        SkyPolyline line;
        // Muestreo denso cada 1 grado para evitar vacíos o líneas cortadas
        for (int latDeg = minLatDeg; latDeg <= 85; latDeg += 1)
        {
            Vec3d v;
            StelUtils::spheToRect(lon, latDeg * DEG2RAD, v);
            Vec3d screen;
            if (projector->project(v, screen))
            {
                line.points.append(QPointF(
                    (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                    (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
            }
        }

        if (line.points.size() >= 2)
        {
            // Meridiano de referencia (Az = 0°, Norte): mismo criterio que en
            // la cuadrícula ecuatorial, trazo un poco más ancho para
            // destacarlo del resto de la cuadrícula.
            const bool isPrimeMeridian = (i == 0);

            line.category          = 1;
            line.type              = SkyPolylineType::GridLine;
            line.width             = isPrimeMeridian ? 0.7f : 0.35f;
            line.color             = QColor(120, 120, 120);
            line.style             = Qt::SolidLine;
            line.labelText         = QString("%1°").arg(int(lonDeg));
            line.isMeridianLine    = true;
            line.truncatedByHorizon = cutByHorizon;

            scene.polylines.push_back(line);
        }
    }

    // ================= Paralelos de altura constante (en grados) =================
    const double latStepDeg = qMax(1.0, stepDeg);
    for (double latDegD = minLatDeg; latDegD <= 75.0; latDegD += latStepDeg)
    {
        const int latDeg = int(latDegD);
        const double lat = latDeg * DEG2RAD;

        SkyPolyline line;
        // Muestreo horizontal continuo cada 1 grado
        for (int lonDeg = 0; lonDeg <= 360; lonDeg += 1)
        {
            const double adjustedLonDeg = fmod(lonDeg + 180.0, 360.0);
            Vec3d v;
            StelUtils::spheToRect(adjustedLonDeg * DEG2RAD, lat, v);
            Vec3d screen;
            if (projector->project(v, screen))
            {
                line.points.append(QPointF(
                    (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
                    (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
            }
        }

        if (line.points.size() >= 2)
        {
            line.category          = 1;
            line.type              = SkyPolylineType::GridLine;
            line.width             = 0.35f;
            line.color             = QColor(120, 120, 120);
            line.style             = Qt::SolidLine;
            line.labelText         = QString("%1%2°").arg(latDeg >= 0 ? "+" : "").arg(latDeg);
            line.isMeridianLine    = false;
            line.truncatedByHorizon = cutByHorizon;

            scene.polylines.push_back(line);
        }
    }
}

//*********************************************************************

//*********************************************************************

bool SkySceneExtractor::isAboveHorizon(const Vec3d& j2000Pos) const
{
    if (!m_core) return true;
    const Vec3d altAz = m_core->j2000ToAltAz(j2000Pos, StelCore::RefractionOff);
    return altAz[2] > 0.0;
}

//*********************************************************************
// Helper: lee el index.json de la cultura del cielo activa y arma las
// líneas de constelación como SkyPolyline. ConstellationMgr no expone
// esta geometría (es privada), así que la extraemos directamente del
// archivo de datos, igual que hace Stellarium internamente al cargar
// la cultura.
//*********************************************************************

void SkySceneExtractor::buildConstellationLines(SkyScene& scene)
{
    if (!m_core || !m_starMgr) return;

    auto* skyCultureMgr = GETSTELMODULE(StelSkyCultureMgr);
    if (!skyCultureMgr) return;

    const QString cultureId = skyCultureMgr->getCurrentSkyCultureID();
    if (cultureId.isEmpty()) return;

    const QString indexPath = StelFileMgr::findFile(
        QString("skycultures/%1/index.json").arg(cultureId));

    if (indexPath.isEmpty())
    {
        qWarning() << "SkySceneExtractor: no se encontró index.json para la cultura" << cultureId;
        return;
    }

    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "SkySceneExtractor: no se pudo abrir" << indexPath;
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "SkySceneExtractor: error parseando index.json:" << parseError.errorString();
        return;
    }

    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    const QJsonArray constellationsArray = doc.object().value("constellations").toArray();

    int lineCount = 0;
    for (const QJsonValue& consVal : constellationsArray)
    {
        const QJsonObject consObj = consVal.toObject();
        const QJsonArray linesArray = consObj.value("lines").toArray(); // <- clave a verificar si da 0

        for (const QJsonValue& segmentVal : linesArray)
{
    const QJsonArray hipChain = segmentVal.toArray();
    if (hipChain.size() < 2) continue;

    SkyPolyline currentLine;
    for (const QJsonValue& hipVal : hipChain)
    {
        const int hip = hipVal.toInt(-1);
        if (hip < 0) continue;

        StelObjectP star = m_starMgr->searchHP(hip);
        if (!star) continue;

        Vec3d position = star->getJ2000EquatorialPos(m_core);

        if (scene.state.horizonLineVisible && !isAboveHorizon(position))
{
    if (currentLine.points.size() >= 2)
    {
        currentLine.category = 3; // constelación (legado)
        currentLine.type     = SkyPolylineType::ConstellationLine;
        currentLine.width    = 0.4f;
        currentLine.color    = QColor(80, 100, 140);
        currentLine.style    = Qt::SolidLine;

        scene.polylines.push_back(currentLine);
    }
    currentLine = SkyPolyline();
    continue;
}

        Vec3d screen;
        if (!projector->project(position, screen)) continue;

        currentLine.points.append(QPointF(
            (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
            (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
    }

        if (currentLine.points.size() >= 2)
{
    currentLine.category = 3;
    currentLine.type     = SkyPolylineType::ConstellationLine;
    currentLine.width    = 0.4f;
    currentLine.color    = QColor(80, 100, 140);
    currentLine.style    = Qt::SolidLine;

    scene.polylines.push_back(currentLine);
    ++lineCount;
}

    }
}

    qDebug() << "SkySceneExtractor:" << lineCount
             << "constellation line segments loaded from" << indexPath;
}

//*********************************************************************

//*********************************************************************

QColor SkySceneExtractor::starColor(float bv) const
{
    // Escala física aproximada de temperatura estelar
    if (bv < -0.2f) return QColor(155, 176, 255); // O
    if (bv < 0.0f)  return QColor(170, 191, 255); // B
    if (bv < 0.3f)  return QColor(202, 215, 255); // A
    if (bv < 0.6f)  return QColor(255, 244, 234); // F
    if (bv < 1.0f)  return QColor(255, 210, 161); // G
    if (bv < 1.5f)  return QColor(255, 167,  90); // K
    return QColor(255, 120,  60);                 // M
}


//*********************************************************************

//*********************************************************************

void SkySceneExtractor::buildHorizonLine(SkyScene& scene)
{
    if (!m_core) return;
    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    
    SkyPolyline line;

    for (int azDeg = 0; azDeg <= 360; azDeg += 2)
    {
        Vec3d altAzPos;
        StelUtils::spheToRect(azDeg * DEG2RAD, 0.0, altAzPos); // altitud = 0 -> horizonte exacto
        const Vec3d j2000Pos = m_core->altAzToJ2000(altAzPos);

        Vec3d screen;
        if (!projector->project(j2000Pos, screen)) continue;
        line.points.append(QPointF(
            (screen[0] / scene.viewportSize.width()) * 2.0 - 1.0,
            (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
    }

        if (line.points.size() >= 2)
{
    line.category = 2; // horizonte (legado)
    line.type     = SkyPolylineType::HorizonLine;
    line.width    = 1.5f;
    line.color    = QColor(40, 40, 40);
    line.style    = Qt::DashLine;

    scene.polylines.push_back(line);
}
}

//*********************************************************************

//*********************************************************************

void SkySceneExtractor::buildEclipticLine(SkyScene& scene, bool ofDate)
{
    if (!m_core) return;

    const StelCore::FrameType frame = ofDate
        ? StelCore::FrameObservercentricEclipticOfDate
        : StelCore::FrameObservercentricEclipticJ2000;

    StelProjectorP projector = m_core->getProjection(frame);
    if (!projector) return;

    // Oblicuidad a usar para poder convertir el punto eclíptico a
    // ecuatorial J2000 y así reutilizar isAboveHorizon().
    double obliquityRad;
    if (ofDate)
    {
        auto currentPlanet = m_core->getCurrentPlanet();
        if (!currentPlanet) return;
        obliquityRad = currentPlanet->getRotObliquity(m_core->getJDE());
    }
    else
    {
        obliquityRad = 23.4392911 * DEG2RAD; // oblicuidad J2000, constante fija
    }

    SkyPolyline line;
    line.labelText = ofDate ? q_("Ecliptic of date") : q_("Ecliptic J2000");

    Vec3d prevJ2000;
    bool havePrev = false;
    bool prevAbove = true;
    int prevLonDeg = -2;
    bool segmentTruncatedByHorizon = false;   // NUEVO

   auto flush = [&]()
    {
        if (line.points.size() >= 2)
        {
            line.category = ofDate ? 5 : 4;
            line.type  = SkyPolylineType::EclipticLine;
            line.width = 1.0f;
            line.color = ofDate ? QColor(180, 120, 40) : QColor(200, 150, 60);
            line.style = Qt::DashDotLine;
            // NUEVO: misma convención que la cuadrícula -> ancla en el
            // marco invisible, preferencia por el borde SUPERIOR.
            line.isMeridianLine     = true;
            line.truncatedByHorizon = segmentTruncatedByHorizon;
            scene.polylines.push_back(line);
        }
        line = SkyPolyline();
        line.labelText = ofDate ? q_("Ecliptic of date") : q_("Ecliptic J2000");
        segmentTruncatedByHorizon = false; // NUEVO: reset para el siguiente tramo
    };

    for (int lonDeg = 0; lonDeg <= 360; lonDeg += 2)
    {
        // Punto en el frame eclíptico correspondiente, para proyectar en pantalla:
        Vec3d eclPos;
        StelUtils::spheToRect(lonDeg * DEG2RAD, 0.0, eclPos);

        // El MISMO punto pero convertido a ecuatorial J2000, solo para
        // poder preguntarle a isAboveHorizon() si está sobre el horizonte.
        double raRad, decRad;
        StelUtils::eclToEqu(lonDeg * DEG2RAD, 0.0, obliquityRad, &raRad, &decRad);
        Vec3d j2000Pos;
        StelUtils::spheToRect(raRad, decRad, j2000Pos);

        bool above = !scene.state.horizonLineVisible || isAboveHorizon(j2000Pos);

        if (havePrev && above != prevAbove)
        {
            const Vec3d prevAltAz = m_core->j2000ToAltAz(prevJ2000, StelCore::RefractionOff);
            const Vec3d currAltAz = m_core->j2000ToAltAz(j2000Pos, StelCore::RefractionOff);
            const double z0 = prevAltAz[2];
            const double z1 = currAltAz[2];
            if (z1 != z0)
            {
                const double t = z0 / (z0 - z1);
                const double crossLonDeg = prevLonDeg + t * (lonDeg - prevLonDeg);
                Vec3d crossEcl;
                StelUtils::spheToRect(crossLonDeg * DEG2RAD, 0.0, crossEcl);
                Vec3d crossScreen;
              if (projector->project(crossEcl, crossScreen))
                {
                    line.points.append(QPointF(
                        (crossScreen[0] / scene.viewportSize.width())  * 2.0 - 1.0,
                        (crossScreen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
                }
            }
            segmentTruncatedByHorizon = true; // NUEVO
            flush();
        }

        if (above)
        {
            Vec3d screen;
            if (projector->project(eclPos, screen))
            {
                line.points.append(QPointF(
                    (screen[0] / scene.viewportSize.width())  * 2.0 - 1.0,
                    (screen[1] / scene.viewportSize.height()) * 2.0 - 1.0));
            }
        }

        prevJ2000 = j2000Pos;
        prevLonDeg = lonDeg;
        prevAbove = above;
        havePrev = true;
    }
    flush();
}


//*********************************************************************

//*********************************************************************


SkySceneState SkySceneExtractor::captureCurrentState()
{
    m_core         = StelApp::getInstance().getCore();
    m_starMgr      = GETSTELMODULE(StarMgr);
    m_constMgr     = GETSTELMODULE(ConstellationMgr);
    m_nebulaMgr    = GETSTELMODULE(NebulaMgr);
    m_solarSystem  = GETSTELMODULE(SolarSystem);
    m_gridMgr      = GETSTELMODULE(GridLinesMgr);

    SkyScene tmp;
    captureModuleState(tmp);
    return tmp.state;
}



//*********************************************************************
// TRAZADOR DE CONTORNOS (Marching Squares) — solo para la Vía Láctea
// VECTORIAL (scene.milkyWayBands, usada por PDF/SVG). No toca el camino
// raster (scene.milkyWayImage), que sigue igual que antes para
// JPEG/PNG/TIFF.
//*********************************************************************
namespace {

struct MSPoint { double x; double y; };

// Cuantizamos a 1/1000 de celda de grilla: alcanza para que dos segmentos
// vecinos que comparten el mismo cruce de arista generen exactamente la
// misma clave y puedan enlazarse en un único contorno.
quint64 msKey(double x, double y)
{
    qint32 ix = static_cast<qint32>(std::llround(x * 1000.0));
    qint32 iy = static_cast<qint32>(std::llround(y * 1000.0));
    return (static_cast<quint64>(static_cast<quint32>(ix)) << 32) | static_cast<quint32>(iy);
}

double lerpEdge(double v0, double v1, double thr)
{
    if (std::abs(v1 - v0) < 1e-9) return 0.5;
    return qBound(0.0, (thr - v0) / (v1 - v0), 1.0);
}

// Traza todos los contornos CERRADOS del campo escalar 'field' (tamaño
// (h+2) x (w+2): ya incluye un borde de padding en 0, ver el llamador)
// para el umbral 'thr'. Como el padding garantiza que el valor nunca
// supera el umbral en el borde de la grilla, cada punto de cruce queda
// compartido por EXACTAMENTE 2 segmentos → el enlazado por punto siempre
// cierra en un loop, sin casos de contorno abierto que resolver.
QVector<QPolygonF> traceContours(const QVector<QVector<double>>& field, int w, int h, double thr)
{
    struct Seg { MSPoint p0, p1; };
    QVector<Seg> segments;
    segments.reserve((w * h) / 4);

    for (int j = 0; j <= h; ++j)
    {
        for (int i = 0; i <= w; ++i)
        {
            const double vTL = field[j][i];
            const double vTR = field[j][i + 1];
            const double vBL = field[j + 1][i];
            const double vBR = field[j + 1][i + 1];

            const bool a = vTL >= thr; // TL
            const bool b = vTR >= thr; // TR
            const bool c = vBR >= thr; // BR
            const bool d = vBL >= thr; // BL

            if (a == b && b == c && c == d)
                continue; // celda uniforme (toda dentro o toda fuera): sin cruce

            const MSPoint top    { i + lerpEdge(vTL, vTR, thr), (double)j };
            const MSPoint right  { (double)(i + 1), j + lerpEdge(vTR, vBR, thr) };
            const MSPoint bottom { i + lerpEdge(vBL, vBR, thr), (double)(j + 1) };
            const MSPoint left   { (double)i, j + lerpEdge(vTL, vBL, thr) };

            const bool eTop    = (a != b);
            const bool eRight  = (b != c);
            const bool eBottom = (d != c);
            const bool eLeft   = (a != d);
            const int activeCount = (eTop?1:0) + (eRight?1:0) + (eBottom?1:0) + (eLeft?1:0);

            if (activeCount == 2)
            {
                MSPoint p0{}, p1{};
                int found = 0;
                if (eTop)    { (found++ == 0 ? p0 : p1) = top; }
                if (eRight)  { (found++ == 0 ? p0 : p1) = right; }
                if (eBottom) { (found++ == 0 ? p0 : p1) = bottom; }
                if (eLeft)   { (found++ == 0 ? p0 : p1) = left; }
                segments.push_back({p0, p1});
            }
            else if (activeCount == 4)
            {
                // Caso "silla de montar" (esquinas opuestas en el mismo
                // estado). Desempate estándar según qué diagonal está
                // "dentro": a esta resolución de grilla, ya pensada para
                // bandas de estilo cartográfico (no para reproducir la
                // textura al píxel), el error visual de esta aproximación
                // es despreciable.
                if (a) { segments.push_back({left, top});  segments.push_back({right, bottom}); }
                else   { segments.push_back({top, right}); segments.push_back({bottom, left});  }
            }
        }
    }

    QMultiMap<quint64, int> byPoint;
    for (int s = 0; s < segments.size(); ++s)
    {
        byPoint.insert(msKey(segments[s].p0.x, segments[s].p0.y), s);
        byPoint.insert(msKey(segments[s].p1.x, segments[s].p1.y), s);
    }

    QVector<bool> used(segments.size(), false);
    QVector<QPolygonF> contours;

    for (int s0 = 0; s0 < segments.size(); ++s0)
    {
        if (used[s0]) continue;
        used[s0] = true;

        QPolygonF loop;
        const MSPoint startPt = segments[s0].p0;
        MSPoint curPt = segments[s0].p1;
        loop << QPointF(startPt.x, startPt.y) << QPointF(curPt.x, curPt.y);

        const int maxSteps = segments.size() + 4;
        int steps = 0;
        while (steps++ < maxSteps)
        {
            const quint64 key = msKey(curPt.x, curPt.y);
            int nextSeg = -1;
            const auto candidates = byPoint.values(key);
            for (int cand : candidates) { if (!used[cand]) { nextSeg = cand; break; } }
            if (nextSeg < 0) break;

            used[nextSeg] = true;
            const Seg& sg = segments[nextSeg];
            const bool p0IsCur = (msKey(sg.p0.x, sg.p0.y) == key);
            const MSPoint nextPt = p0IsCur ? sg.p1 : sg.p0;

            if (msKey(nextPt.x, nextPt.y) == msKey(startPt.x, startPt.y))
                break; // contorno cerrado

            loop << QPointF(nextPt.x, nextPt.y);
            curPt = nextPt;
        }

        if (loop.size() >= 3)
            contours.push_back(loop);
    }

    return contours;
}

// Simplificación Douglas-Peucker liviana: sin esto, cada contorno arrastra
// ~cientos de vértices casi colineales y el PDF/SVG resultante pesa mucho
// más de lo necesario para una banda de densidad estilizada.
void simplifyDP(const QPolygonF& in, double epsilon, QVector<QPointF>& out, int start, int end)
{
    if (end <= start + 1) { out.push_back(in[start]); return; }

    double maxDist = -1.0;
    int idx = start;
    const QPointF& a = in[start];
    const QPointF& b = in[end];
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;

    for (int i = start + 1; i < end; ++i)
    {
        double dist;
        if (len2 < 1e-12)
            dist = QLineF(a, in[i]).length();
        else
        {
            double t = qBound(0.0, ((in[i].x() - a.x()) * dx + (in[i].y() - a.y()) * dy) / len2, 1.0);
            dist = QLineF(QPointF(a.x() + t * dx, a.y() + t * dy), in[i]).length();
        }
        if (dist > maxDist) { maxDist = dist; idx = i; }
    }

    if (maxDist > epsilon)
    {
        simplifyDP(in, epsilon, out, start, idx);
        simplifyDP(in, epsilon, out, idx, end);
    }
    else
    {
        out.push_back(in[start]);
    }
}

QPolygonF simplifyPolygon(const QPolygonF& poly, double epsilon)
{
    if (poly.size() < 5) return poly;
    QVector<QPointF> reduced;
    simplifyDP(poly, epsilon, reduced, 0, poly.size() - 1);
    reduced.push_back(poly.last());
    return QPolygonF(reduced);
}

} // namespace

//*********************************************************************
// Extrae la textura de la Vía Láctea y la proyecta en una capa 2D
// con un color transparente, respetando el horizonte arqueológico.
//
// Genera DOS representaciones a partir del mismo muestreo de textura:
//   - scene.milkyWayImage : raster (igual que antes), para JPEG/PNG/TIFF.
//   - scene.milkyWayBands : vectorial (bandas de isodensidad como
//     polígonos), para PDF/SVG. Ver comentario en SkyScene.hpp.
//*********************************************************************
void SkySceneExtractor::captureMilkyWay(SkyScene& scene)
{
    if (!scene.state.milkyWayVisible || !m_core) return;

    // 1. Ruta absoluta de instalación (más seguro que findFile)
    QString texPath = StelFileMgr::getInstallationDir() + "/textures/milkyway.png";
    QImage mwTex(texPath);
    if (mwTex.isNull()) return;

    StelProjectorP projector = m_core->getProjection(StelCore::FrameJ2000);
    if (!projector) return;

    // ------------------------------------------------------------------
    // Muestreo común: dado un punto en píxeles de viewport, devuelve el
    // alfa final (0..scene.state.milkyWayAlpha) tal como lo pintaba el
    // código raster original.
    // ------------------------------------------------------------------
    auto sampleAlpha = [&](double px, double py) -> double
    {
        Vec3d j2000Dir;
        if (!projector->unProject(px, py, j2000Dir))
            return 0.0;

        if (scene.state.horizonLineVisible && !isAboveHorizon(j2000Dir))
            return 0.0;

        j2000Dir.normalize();

        // 1. MATEMÁTICA NATIVA DE STELLARIUM
        // Extraída directamente del shader MilkyWay.cpp de Stellarium
        double modelZenithAngle = std::acos(-j2000Dir[2]);
        
        // CUIDADO AQUÍ: En GLSL la función atan(x,y) equivale a std::atan2(X, Y) en C++
        double modelLongitude = std::atan2(j2000Dir[0], j2000Dir[1]);

        // 2. MAPEO HORIZONTAL (U)
        double u = modelLongitude / (2.0 * M_PI);
        if (u < 0.0) u += 1.0;

        // 3. MAPEO VERTICAL (V) - LA CORRECCIÓN CRUCIAL
        // OpenGL lee v=0 desde ABAJO. Qt (QImage) lee y=0 desde ARRIBA.
        // Invertimos V (1.0 - ...) para evitar que la banda se dibuje de cabeza,
        // lo cual causaba que se desfasara "más abajo" y arqueara al revés.
        double v = 1.0 - (modelZenithAngle / M_PI);

        int tx = qBound(0, (int)(u * mwTex.width()), mwTex.width() - 1);
        int ty = qBound(0, (int)(v * mwTex.height()), mwTex.height() - 1);

        QRgb texColor = mwTex.pixel(tx, ty);
        int brightness = qGray(texColor);

        // Omitimos el fondo negro absoluto del espacio para evitar recuadros
        if (brightness <= 8) return 0.0;

        // Compresión de contraste cartográfico
        double normB = (brightness - 8) / 247.0;
        double flatB = std::pow(normB, 0.4);

        return qBound(0.0, flatB * (double)scene.state.milkyWayAlpha, 255.0);
    };

    // ==================================================================
    // 1) RASTER — igual que antes (usado por JPEG/PNG/TIFF y por el
    //    canvas de PreviewDialog cuando se previsualiza un formato raster)
    // ==================================================================
    const int maxDim = 800;
    int imgW = maxDim;
    int imgH = maxDim;
    if (scene.viewportSize.width() > scene.viewportSize.height()) {
        imgH = (scene.viewportSize.height() / scene.viewportSize.width()) * maxDim;
    } else {
        imgW = (scene.viewportSize.width() / scene.viewportSize.height()) * maxDim;
    }

    QImage outImg(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    outImg.fill(Qt::transparent);

    for (int y = 0; y < imgH; ++y)
    {
        for (int x = 0; x < imgW; ++x)
        {
            // CORRECCIÓN MATEMÁTICA: Usar (imgW - 1) para cubrir de borde a borde sin cortes
            double px = (x / (double)(imgW - 1)) * scene.viewportSize.width();
            double py = ((imgH - 1.0 - y) / (double)(imgH - 1)) * scene.viewportSize.height();

            double alpha = sampleAlpha(px, py);
            if (alpha > 0.0)
                outImg.setPixelColor(x, y, QColor(100, 120, 150, (int)alpha));
        }
    }
    scene.milkyWayImage = outImg;

    // ==================================================================
    // 2) VECTORIAL — bandas de isodensidad para PDF/SVG (scene.milkyWayBands).
    //    Grilla mucho más gruesa que el raster a propósito: los contornos
    //    se simplifican de todas formas, así que una grilla fina solo
    //    agregaría vértices sin mejorar el resultado vectorial.
    // ==================================================================
    scene.milkyWayBands.clear();

    const int maxDimVec = 160;
    int vecW = maxDimVec;
    int vecH = maxDimVec;
    if (scene.viewportSize.width() > scene.viewportSize.height()) {
        vecH = qMax(2, (int)((scene.viewportSize.height() / scene.viewportSize.width()) * maxDimVec));
    } else {
        vecW = qMax(2, (int)((scene.viewportSize.width() / scene.viewportSize.height()) * maxDimVec));
    }

    // Campo con un borde de padding en 0 alrededor: garantiza que
    // traceContours() siempre devuelva contornos CERRADOS (el valor nunca
    // supera el umbral en el borde de la grilla), sin casos especiales de
    // contorno que "sale" del área capturada.
    QVector<QVector<double>> field(vecH + 2, QVector<double>(vecW + 2, 0.0));
    for (int gy = 0; gy < vecH; ++gy)
    {
        for (int gx = 0; gx < vecW; ++gx)
        {
            double px = (gx / (double)(vecW - 1)) * scene.viewportSize.width();
            double py = ((vecH - 1.0 - gy) / (double)(vecH - 1)) * scene.viewportSize.height();
            field[gy + 1][gx + 1] = sampleAlpha(px, py);
        }
    }

    const double maxAlpha = (double)scene.state.milkyWayAlpha;
    if (maxAlpha > 0.0)
    {
        const int kBandCount = 5; // bandas de tenue/grande a brillante/pequeña
        const double epsilon = 0.35; // en unidades de celda de grilla

        for (int band = 1; band <= kBandCount; ++band)
        {
            const double thr = maxAlpha * (double)band / (double)(kBandCount + 1);
            QVector<QPolygonF> loops = traceContours(field, vecW, vecH, thr);
            if (loops.isEmpty()) continue;

            SkyMilkyWayBand mwBand;
            // Alfa incremental por banda: pintadas de la tenue a la
            // brillante, la composición "over" de Qt se acerca al
            // gradiente continuo del raster, en forma de banda escalonada
            // (el mismo estilo que usan los atlas impresos clásicos para
            // representar la Vía Láctea).
            const int bandAlpha = qBound(1, (int)(maxAlpha / (double)(kBandCount + 1)), 255);
            mwBand.color = QColor(100, 120, 150, bandAlpha);

            for (const QPolygonF& loop : loops)
            {
                QPolygonF simplified = simplifyPolygon(loop, epsilon);
                if (simplified.size() < 3) continue;

                // Coordenadas de grilla (incluyen el offset de padding +1)
                // -> UV [0,1] relativo al viewport capturado. Fila 0 =
                // arriba, igual que en el raster (outImg / drawImage).
                QPolygonF uvLoop;
                uvLoop.reserve(simplified.size());
                for (const QPointF& gp : simplified)
                {
                    double u = (gp.x() - 1.0) / (double)(vecW - 1);
                    double v = (gp.y() - 1.0) / (double)(vecH - 1);
                    uvLoop << QPointF(u, v);
                }
                mwBand.contours.push_back(uvLoop);
            }

            if (!mwBand.contours.isEmpty())
                scene.milkyWayBands.push_back(mwBand);
        }
    }
}