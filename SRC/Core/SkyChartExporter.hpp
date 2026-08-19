#pragma once
#include <QObject>
#include <QString>
#include "SkyScene.hpp"
#include "StelModule.hpp"
#include "StelPluginInterface.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "SkyChartExporterOptions.hpp"

class SkyChartExporterDialog;
class StelButton;
class QPainter;
class QPdfWriter;
class QImage;
class QRectF;

class SkyChartExporter : public StelModule
{
    Q_OBJECT

public:
    explicit SkyChartExporter(QObject* parent = nullptr);
    ~SkyChartExporter() override = default;

    void init() override;
    void deinit() override;
    double getCallOrder(StelModuleActionName a) const override;
    void update(double deltaTime) override { Q_UNUSED(deltaTime) }

    SkyScene generarPreview(QImage& outImage, int w, int h,
                            const SkyChartExportOptions& options);

    void exportarConDialogo();

    static void configureWriter(QPdfWriter& writer,
                                const SkyChartExportOptions& options);

    bool exportToPdf(const QString& filePath,
                     const SkyChartExportOptions& options);

    bool exportToImage(const QString& filePath, const SkyChartExportOptions& options);

    bool exportChart(const QString& filePath, const SkyChartExportOptions& options); // dispatcher PDF/Imagen
    
   bool exportToSvg(const QString& filePath, const SkyChartExportOptions& options);
   
 bool paintChart(QPainter& painter,
                    const SkyChartExportOptions& options,
                    SkyScene* outScene = nullptr,
                    QRectF* outContentRect = nullptr);

signals:
    void progresoExportacion(int porcentaje, const QString& etapa);
    void exportacionCompletada(bool ok, const QString& filePath);

private:
    // === UI ===
    SkyChartExporterDialog* configDialog  = nullptr;
    StelButton*             toolbarButton = nullptr;

    // === CACHEO DE ESCENA ===
    SkyScene m_cachedScene;
    bool m_sceneCacheValid = false;
    SkyChartExportOptions m_cachedOptions;
    double m_cachedJD = 0.0;
    double m_cachedFov = 0.0;

    bool m_cachedDesignationUsage = false;

    bool cacheMatches(const SkyChartExportOptions& options) const;
};

// ==========================================================
// INTERFAZ DEL PLUGIN (FUERA de la clase, nunca anidada)
// ==========================================================
class SkyChartExporterStelPluginInterface : public QObject, public StelPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
    Q_INTERFACES(StelPluginInterface)

public:
    StelModule* getStelModule() const override;
    StelPluginInfo getPluginInfo() const override;
};