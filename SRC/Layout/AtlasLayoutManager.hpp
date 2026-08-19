#pragma once
#include <QRectF>
#include <QSizeF>
#include <QPainter>
#include "SkyScene.hpp"
#include "SkyChartExporterOptions.hpp"

class AtlasLayoutManager
{
public:
    AtlasLayoutManager();

    // Calcula el rectángulo total de la página en puntos (1 mm = 2.83465 pt)
    QRectF buildPageRect(const SkyChartExportOptions& options);

    // Calcula la región imprimible/útil descontando los 4 márgenes de la interfaz
    QRectF getContentRect(const SkyChartExportOptions& options) const;

    QSizeF pageSizePx() const;

    // Renderiza marco, encabezados y leyendas según las opciones seleccionadas
    void renderLayout(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options);
    

private:
    void renderHeader(QPainter& painter, const SkyChartExportOptions& options);
    void renderFooter(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options);
    void renderLegends(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options);
    void renderSpectralLegend(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options);
    void renderCoordinateSystemLabel(QPainter& painter, const SkyScene& scene, const SkyChartExportOptions& options);
    void renderNorthArrow(QPainter& painter, const QRectF& rect, const SkyScene& scene);

    double m_spectralLegendHeight = 60.0;

    // Colchón mínimo (en pt) que SIEMPRE debe quedar entre cualquier caja de
    // leyenda/etiqueta y el borde físico real de la página (m_pageRect), sin
    // importar el margen configurado por el usuario. Antes las cajas solo se
    // posicionaban contra m_contentRect, que con margen=0 es idéntico a
    // m_pageRect, dejando solo el offset fijo (10-15pt) como único colchón
    // real -> se veía el corte. Con este mínimo, aunque el margen del
    // usuario sea 0, la leyenda nunca queda a menos de esta distancia del
    // borde real de la hoja.
    static constexpr double kMinPageEdgeMargin = 15.0;

    QRectF m_pageRect;
    QRectF m_contentRect;
};