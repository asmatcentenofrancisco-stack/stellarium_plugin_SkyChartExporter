#include "SvgExporter.hpp"
#include "AtlasLayoutManager.hpp"

#include <QSvgGenerator>

void SvgExporter::configureGenerator(QSvgGenerator& generator, const SkyChartExportOptions& options)
{
    AtlasLayoutManager layout;
    QRectF pageRect = layout.buildPageRect(options); // mismo tamaño en puntos que usa el PDF

    generator.setSize(pageRect.size().toSize());
    generator.setViewBox(pageRect);

    // Debe coincidir con mmToPt de AtlasLayoutManager (72 pt/pulgada), igual
    // que PdfExporter::configureWriter. Es solo metadata nominal: SVG en sí
    // es resoluble a cualquier tamaño sin pérdida.
    generator.setResolution(72);

    generator.setTitle(options.title.isEmpty()
                        ? QStringLiteral("Sky Chart")
                        : options.title);
    generator.setDescription(QStringLiteral("Generado por SkyChartExporter (Stellarium)"));
}