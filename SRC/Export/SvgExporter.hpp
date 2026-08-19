#ifndef SVGEXPORTER_HPP
#define SVGEXPORTER_HPP

/*
 * SkyChartExporter Plugin
 * Stellarium 26.1
 *
 * SvgExporter.hpp
 *
 * "La Imprenta" para el formato vectorial SVG.
 *
 * SVG comparte el mismo motor de dibujo que el PDF: SkyChartRenderer +
 * AtlasLayoutManager pintan sobre un QPainter, sin importarles si el
 * QPaintDevice de abajo es un QPdfWriter o un QSvgGenerator. Por eso esta
 * clase, igual que PdfExporter, solo se encarga de CONFIGURAR el
 * dispositivo (tamaño de página, resolución nominal, metadata) — no dibuja
 * nada. El dibujo real lo sigue haciendo SkyChartExporter::paintChart().
 *
 * SVG es resolución-independiente: no usa DPI ni ancho/alto en píxeles,
 * a diferencia de ImageExporter (JPEG/PNG/TIFF).
 */

#include "SkyChartExporterOptions.hpp"

class QSvgGenerator;

class SvgExporter
{
public:

    // Configura tamaño de página, resolución nominal (debe coincidir con
    // AtlasLayoutManager, igual que hace PdfExporter::configureWriter) y
    // metadata del archivo SVG. Equivalente a PdfExporter::configureWriter.
    static void configureGenerator(QSvgGenerator& generator, const SkyChartExportOptions& options);
};

#endif // SVGEXPORTER_HPP