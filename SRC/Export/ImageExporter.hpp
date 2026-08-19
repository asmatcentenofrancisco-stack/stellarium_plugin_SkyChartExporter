#ifndef IMAGEEXPORTER_HPP
#define IMAGEEXPORTER_HPP

/*
 * SkyChartExporter Plugin
 * Stellarium 26.1
 *
 * ImageExporter.hpp
 *
 * "La Imprenta" para formatos de imagen (PNG/JPEG/TIFF).
 * Equiparable a PdfExporter, pero en vez de escribir sobre un QPdfWriter,
 * construye un QImage con el tamaño de página correcto en píxeles
 * (según DPI) y lo guarda con QImage::save().
 *
 * No dibuja (eso lo hacen SkyChartRenderer y AtlasLayoutManager).
 * No conoce el diálogo ni el orquestador.
 */

#include <QString>
#include <QRectF>
#include <QImage>

#include "SkyChartExporterOptions.hpp"

struct SkyScene;


class ImageExporter
{
public:

    ImageExporter();

    //--------------------------------------------------
    // API simple (equivalente a PdfExporter::exportScene)
    //--------------------------------------------------

    void setOutputPath(const QString& path);
    void setResolution(int dpi);

    bool exportScene(const SkyScene& scene);

    //--------------------------------------------------
    // API avanzada con opciones completas de maquetación
    // (equivalente a PdfExporter::exportSceneWithOptions)
    //--------------------------------------------------

    bool exportSceneWithOptions(const QString& filePath,
                                 const SkyScene& scene,
                                 const SkyChartExportOptions& options,
                                 const QRectF& pageRect,
                                 const QRectF& contentRect);

    //--------------------------------------------------
    // Método de bajo nivel: crea el QImage de destino ya
    // configurado (tamaño en píxeles + DPI embebido).
    // Equivalente en espíritu a PdfExporter::configureWriter.
    //--------------------------------------------------

    static QImage createTargetImage(const SkyChartExportOptions& options);

private:

    QString m_path;
    int     m_dpi;
};

#endif // IMAGEEXPORTER_HPP