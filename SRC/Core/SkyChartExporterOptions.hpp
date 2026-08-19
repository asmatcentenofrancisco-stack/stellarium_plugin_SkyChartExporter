#ifndef SKYCHARTEXPORTEROPTIONS_HPP
#define SKYCHARTEXPORTEROPTIONS_HPP

#include <QString>
#include <QFont>
#include <QList>

/*
 * SkyChartExporter Plugin
 * Stellarium 26.1
 *
 * SkyChartExporterOptions.hpp
 *
 * Estructura de opciones de exportación.
 *
 * Este archivo NO pertenece a la interfaz (Dialog). Es el "contrato"
 * compartido entre:
 *   - SkyChartExporterDialog (quien lo rellena a partir de los controles)
 *   - SkySceneExtractor      (quien lo usa para filtrar/extraer la escena)
 *   - SkyChartRenderer       (quien lo usa para dibujar)
 *   - AtlasLayoutManager     (quien lo usa para maquetar la hoja)
 *   - PdfExporter / ImageExporter (quienes lo usan para configurar la salida)
 *
 * Antes vivía dentro de SkyChartExporterDialog.hpp; se movió aquí para que
 * ninguna de las clases de la capa de dominio (Extractor, Renderer, Layout,
 * Exporters) dependa del header del diálogo.
 */



struct SpecificAstroRule
{
    QString id;
    QString displayName;
    QString type;
    double limitingMagnitude = 6.5;
    QFont font;
    bool bold = false;
    bool italic = false;
};

struct SkyChartExportOptions
{
    //==================================================
    // Papel
    //==================================================
    QString paperSize = "A4";
    QString orientation = "Vertical";
    QString colorMode = "Color";
    double scale = 1.0;

    //==================================================
    // Márgenes de página (en milímetros)
    //==================================================
    double marginLeft = 15.0;
    double marginRight = 15.0;
    double marginTop = 10.0;
    double marginBottom = 5.0;

    //==================================================
    // Estrellas y Etiquetas
    //==================================================
    double limitingMagnitude = 6.5;
    double starLabelMagnitude = 0.0;
    QFont starLabelFont;

    //==================================================
    // Elementos gráficos
    //==================================================
    bool showNorthArrow = true;
    bool showMagnitudeLegend = true;
    bool showSpectralLegend = true;
    bool showAstroLegend = true; // NUEVO: Leyenda dinámica de símbolos de astros
    bool showHorizon = true;
    bool showMilkyWay = true; // NUEVO: Control de Vía Láctea
    int milkyWayAlpha = 150;  // NUEVO: Transparencia
    bool grayscale = false;
    float planetScale = 1.0f;

    //==================================================
    // Información del plano
    //==================================================
    QString title;
    QString subtitle;

    QFont titleFont;
    QFont subtitleFont;

    QList<SpecificAstroRule> specificTargets;

    //==================================================
    // Formato de salida
    //==================================================
    // Vectoriales (motor: SkyChartRenderer + AtlasLayoutManager directo
    // sobre un QPaintDevice, sin DPI ni resolución en píxeles):
    //   "PDF", "SVG"
    // Raster (motor: ImageExporter -> QImage a un tamaño en píxeles):
    //   "JPEG", "PNG", "TIFF"
    QString exportFormat = "PDF";

    // colorMode: valores permitidos según el formato elegido.
    //   PDF / SVG (vectorial): "Color", "EscalaDeGrises", "Monocromo"
    //   JPEG/PNG/TIFF (raster): "RGB", "sRGB", "EscalaDeGrises", "Monocromo"
    // (CMYK descartado: Qt no tiene color CMYK nativo ni en QPainter ni en QImage)

    // DPI — SOLO tiene efecto para formatos raster (JPEG/PNG/TIFF).
    // PDF y SVG son resolución-independiente y lo ignoran.
    int dpi = 300;

    // Override explícito de resolución en píxeles, solo para raster.
    // 0 = calcular automáticamente desde paperSize + orientation + dpi.
    // >0 = se usa tal cual, ignorando el cálculo de papel/DPI.
    int imageWidthPx = 0;
    int imageHeightPx = 0;
    
    // NUEVO: Nivel de compresión / peso de la imagen (1 a 100)
    int imageQuality = 92;
};

#endif // SKYCHARTEXPORTEROPTIONS_HPP