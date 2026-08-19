#include "PdfExporter.hpp"
#include "AtlasLayoutManager.hpp"
#include "SkyChartRenderer.hpp"
#include <QPdfWriter>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>


PdfExporter::PdfExporter()
    : m_path(""), m_dpi(300), m_pageSize(QPageSize::A4), m_marginsMM(0.0)
{
}

void PdfExporter::setOutputPath(const QString& path) { m_path = path; }
void PdfExporter::setResolution(int dpi)             { m_dpi = dpi; }
void PdfExporter::setPageSize(const QPageSize& size) { m_pageSize = size; }
void PdfExporter::setMargins(double mm)              { m_marginsMM = mm; }

// 1. Método heredado (se le pasa una estructura de opciones por defecto)
bool PdfExporter::exportScene(const SkyScene& scene)
{
    if (m_path.isEmpty()) return false;

    QPdfWriter writer(m_path);
    writer.setPageSize(m_pageSize);
    writer.setResolution(600);

    QPainter painter(&writer);
    if (!painter.isActive()) return false;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRectF pageRect(0, 0,
        painter.device()->width(),
        painter.device()->height());

    SkyChartRenderer renderer;
    // Se agrega el 4to argumento con opciones por defecto para cumplir con la firma del renderizador
    renderer.render(painter, pageRect, scene, SkyChartExportOptions());

    painter.end();
    return true;
}

// 2. Método avanzado con opciones completas de maquetación
bool PdfExporter::exportSceneWithOptions(const QString& filePath,
                                         const SkyScene& scene,
                                         const SkyChartExportOptions& options,
                                         const QRectF& pageRect,
                                         const QRectF& contentRect)
{
    QPdfWriter pdfWriter(filePath);

    QPageSize::PageSizeId pageSizeId = QPageSize::A4;
    if (options.paperSize == "A3") pageSizeId = QPageSize::A3;
    else if (options.paperSize == "A2") pageSizeId = QPageSize::A2;
    else if (options.paperSize == "Carta") pageSizeId = QPageSize::Letter;
    else if (options.paperSize == "Legal") pageSizeId = QPageSize::Legal;

    QPageLayout::Orientation orientation = (options.orientation == "Horizontal")
                                            ? QPageLayout::Landscape
                                            : QPageLayout::Portrait;

    pdfWriter.setPageLayout(QPageLayout(QPageSize(pageSizeId), orientation, QMarginsF(0,0,0,0)));
    pdfWriter.setResolution(72); // <-- antes 300; debe coincidir con mmToPt (72 pt/in) 

    QPainter painter(&pdfWriter);
    if (!painter.isActive())
        return false;

    AtlasLayoutManager layout;
    layout.buildPageRect(options); // necesario para fijar m_contentRect

    SkyChartRenderer renderer;

    renderer.render(painter, contentRect, scene, options); // 1º: fondo + cielo

    layout.renderLayout(painter, scene, options);           // 2º: marco, título, leyendas, norte ENCIMA

    painter.end();
    return true;
}



void PdfExporter::configureWriter(QPdfWriter& writer, const SkyChartExportOptions& options)
{
    QPageSize::PageSizeId pageSizeId = QPageSize::A4;
    if (options.paperSize == "A3") pageSizeId = QPageSize::A3;
    else if (options.paperSize == "A2") pageSizeId = QPageSize::A2;
    else if (options.paperSize == "Carta") pageSizeId = QPageSize::Letter;
    else if (options.paperSize == "Legal") pageSizeId = QPageSize::Legal;

    QPageLayout::Orientation orientation = (options.orientation == "Horizontal")
                                            ? QPageLayout::Landscape
                                            : QPageLayout::Portrait;

    writer.setPageLayout(QPageLayout(QPageSize(pageSizeId), orientation, QMarginsF(0,0,0,0)));
    writer.setResolution(72); // debe coincidir con mmToPt de AtlasLayoutManager
}


