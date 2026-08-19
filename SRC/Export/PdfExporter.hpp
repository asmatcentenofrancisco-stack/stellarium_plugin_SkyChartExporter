#ifndef PDFEXPORTER_H
#define PDFEXPORTER_H

#include <QString>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>

#include "SkyScene.hpp"
#include "SkyChartExporterOptions.hpp"

class PdfExporter
{
public:

    PdfExporter();

    void setOutputPath(const QString& path);
    void setResolution(int dpi);
    void setPageSize(const QPageSize& size);
    void setMargins(double mm);

    //------------------------------------------------------
    // NUEVA API UNIFICADA
    //------------------------------------------------------
    bool exportScene(const SkyScene& scene);


	bool exportSceneWithOptions(const QString& filePath,
                            const SkyScene& scene,
                            const SkyChartExportOptions& options,
                            const QRectF& pageRect,
                            const QRectF& contentRect);


     
    static void configureWriter(QPdfWriter& writer, const SkyChartExportOptions& options);

private:

    void configureWriter(QPdfWriter& writer);

    QRectF calculatePageRect(const QPdfWriter& writer) const;

private:
    QString m_path;    // <--- Esto es lo que debes tener
    int m_dpi;
    QPageSize m_pageSize;
    double m_marginsMM;

    

};
#endif