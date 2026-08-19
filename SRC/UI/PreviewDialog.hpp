#ifndef PREVIEWDIALOG_HPP
#define PREVIEWDIALOG_HPP

#include <QDialog>
#include "SkyChartExporterOptions.hpp" 


// Declaraciones adelantadas
class SkyChartExporter;
class SkyChartExporterDialog;

namespace Ui {
class PreviewDialog;
}

class SkyChartExporter;

class PreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreviewDialog(QWidget* parent = nullptr);
    ~PreviewDialog();

    void updatePreviewData(SkyChartExporter* exp, const SkyChartExportOptions& opt, SkyChartExporterDialog* mainDialog = nullptr);

private slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void exportFile();

private:
    void renderCanvas();

    Ui::PreviewDialog* ui;
    SkyChartExporter* exporter = nullptr;
    SkyChartExporterDialog* mainExporterDialog = nullptr;
    SkyChartExportOptions options;
    double zoomFactor = 1.0;
};

#endif // PREVIEWDIALOG_HPP