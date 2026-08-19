#ifndef SKYCHARTEXPORTERDIALOG_HPP
#define SKYCHARTEXPORTERDIALOG_HPP

#include "StelDialog.hpp"
#include <QFileDialog>
#include <QImage>
#include <QFont>
#include "SkyChartExporterOptions.hpp"

class SkyChartExporter;
class QEvent;

namespace Ui
{
class SkyChartExporterDialog;
}

class PreviewDialog;

class SkyChartExporterDialog : public StelDialog
{
    Q_OBJECT

public:

    SkyChartExporterDialog();

    ~SkyChartExporterDialog() override;

    void setVisible(bool visible) override;

    void retranslate() override;

protected:

    // Falla 2: reacomoda el diálogo cuando la ventana principal de Stellarium
    // cambia de tamaño. Sin esto, la posición (proxy->pos()) queda fija en
    // coordenadas absolutas guardadas en config.ini; si la ventana se achica,
    // esa posición puede quedar fuera del área visible y el diálogo
    // "desaparece" hasta que se agranda de nuevo la ventana.
    bool eventFilter(QObject* watched, QEvent* event) override;

public slots:

    void exportPdf();

protected:

    void createDialogContent() override;

private slots:

    void updateOptions();

    void updatePreview();

    void applyNoMargins();

    void applyDefaultMargins();
    
    // Funciones dinámicas para activar/desactivar opciones de la interfaz según el formato
    void setFormatToPDF();
    void setFormatToImage();

    void addSelectedObjectFromStellarium();

    void removeSelectedTargetFromList();

   // NUEVO: formato específico por astro seleccionado en la lista
    void loadSelectedTargetFormat();
    void updateSelectedTargetFormat();

    void clearAllSpecificTargets();

private:

    //--------------------------------------------------
    // Inicialización
    //--------------------------------------------------

    void populateCombos();

    void connectSignals();

    void loadDefaults();

    //--------------------------------------------------
    // Utilidades
    //--------------------------------------------------

    void setStatus(const QString& text);

    // NUEVO: dado un HIP, devuelve todos los HIP de la misma constelación
    // (leídos del index.json de la cultura celeste activa)
    QList<int> getConstellationHipsForStar(int hip) const;

private:

    Ui::SkyChartExporterDialog* ui = nullptr;

    SkyChartExporter* exporter = nullptr;

    SkyChartExportOptions options;

    SkyChartExporter* getExporter() const;

    PreviewDialog* previewDialog = nullptr;
};

#endif // SKYCHARTEXPORTERDIALOG_HPP