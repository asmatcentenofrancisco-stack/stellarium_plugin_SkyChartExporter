#include "PreviewDialog.hpp"
#include "ui_PreviewDialog.h"

#include "SkyChartExporter.hpp"
#include "SkyChartRenderer.hpp"
#include "AtlasLayoutManager.hpp"
#include "SkyChartExporterDialog.hpp"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QDebug>
#include <QScrollBar>  
#include <QFileDialog>
#include <QIcon>

PreviewDialog::PreviewDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::PreviewDialog)
{
    ui->setupUi(this);

    // Dimensiones iniciales cómodas
    resize(950, 800);
    setMinimumSize(800, 600);

    setWindowTitle(tr("Vista Previa del Plano - Sky Chart Exporter"));
    setWindowIcon(QIcon(":/mainWindow/icon.png"));

    // NOTA: los window flags (Qt::Window | Qt::WindowStaysOnTopHint) los fija
    // SkyChartExporterDialog::updatePreview() justo después de crear esta
    // instancia (ver el comentario "Aislamiento total" ahí, que explica por
    // qué se crea sin parent y se fuerza a flotar encima de Stellarium).
    // Antes este constructor también llamaba a setWindowFlags() con un juego
    // de flags distinto (Qt::WindowMaximizeButtonHint | WindowCloseButtonHint,
    // propios de una ventana de diálogo genérica), y al fijarse dos veces
    // seguidas sobre la misma ventana recién creada, Qt no terminaba de
    // limpiar el estilo nativo anterior en algunas plataformas: quedaba un
    // frame híbrido con botón de maximizar en vez del look flotante simple
    // que usa el resto de los diálogos de Stellarium. Dejamos que
    // SkyChartExporterDialog sea la única fuente de verdad para los flags.

    // 1. Activar políticas de barras de desplazamiento automática (vertical y horizontal)
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(ui->buttonZoomIn, &QPushButton::clicked, this, &PreviewDialog::zoomIn);
    connect(ui->buttonZoomOut, &QPushButton::clicked, this, &PreviewDialog::zoomOut);
    connect(ui->buttonResetZoom, &QPushButton::clicked, this, &PreviewDialog::resetZoom);
    connect(ui->buttonExportPDF, &QPushButton::clicked, this, &PreviewDialog::exportFile);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &QDialog::reject);
}

PreviewDialog::~PreviewDialog()
{
    delete ui;
}


void PreviewDialog::updatePreviewData(SkyChartExporter* exp, const SkyChartExportOptions& opt, SkyChartExporterDialog* mainDialog)
{
    exporter = exp;
    options = opt;
    mainExporterDialog = mainDialog;
    
    // 1. Calculamos las proporciones reales del plano generado
    AtlasLayoutManager layout;
    QRectF pageRect = layout.buildPageRect(options);
    
    // 2. Extraemos el espacio físico disponible en la ventana (scrollArea), 
    // restando 20 píxeles para generar un pequeño margen de respiración visual.
    double viewWidth = ui->scrollArea->viewport()->width() - 20.0;
    double viewHeight = ui->scrollArea->viewport()->height() - 20.0;
    
    // 3. Calculamos la escala matemática necesaria para "Ajustar a ventana"
    double zoomX = viewWidth / pageRect.width();
    double zoomY = viewHeight / pageRect.height();
    
    // Escogemos el factor de reducción más restrictivo para garantizar que todo 
    // el plano encaje en la vista. Además, usamos qMin con 1.0 para evitar que 
    // papeles pequeños se abran con un zoom gigante mayor al 100%.
    zoomFactor = qMin(qMin(zoomX, zoomY), 1.0);
    
    ui->labelCanvas->clear();      
    renderCanvas();
    ui->scrollArea->horizontalScrollBar()->setValue(0);
    ui->scrollArea->verticalScrollBar()->setValue(0);
}

void PreviewDialog::zoomIn()
{
    zoomFactor *= 1.25;
    renderCanvas();
}

void PreviewDialog::zoomOut()
{
    if (zoomFactor > 0.25)
    {
        zoomFactor /= 1.25;
        renderCanvas();
    }
}

void PreviewDialog::resetZoom()
{
    zoomFactor = 1.0;
    renderCanvas();
}


void PreviewDialog::renderCanvas()
{
    if (!exporter)
        return;

    // 1. Obtener dimensiones reales del papel (solo para tamaño del QImage/zoom)
    AtlasLayoutManager layout;
    QRectF pageRect = layout.buildPageRect(options);

    int targetWidth = static_cast<int>(pageRect.width() * zoomFactor);
    int targetHeight = static_cast<int>(pageRect.height() * zoomFactor);

    QImage image(targetWidth, targetHeight, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(zoomFactor, zoomFactor);

    // 2. Un único punto de verdad para pintar: el mismo que usa la exportación a PDF
    exporter->paintChart(painter, options);

    painter.end();

    ui->labelCanvas->setPixmap(QPixmap::fromImage(image));
    ui->labelCanvas->setFixedSize(targetWidth, targetHeight);
    ui->scrollAreaWidgetContents->resize(targetWidth, targetHeight);

    ui->labelStatus->setText(tr("Zoom: %1% | Dimensiones: %2x%3 px")
        .arg(static_cast<int>(zoomFactor * 100))
        .arg(targetWidth)
        .arg(targetHeight));
}

void PreviewDialog::exportFile()
{
    if (mainExporterDialog)
    {
        mainExporterDialog->exportPdf();
    }
}