/*
 * SkyChartExporter Plugin
 * Stellarium 26.1
 *
 * SkyChartExporterDialog.cpp
 */

#include "SkyChartExporterDialog.hpp"
#include "ui_SkyChartExporterDialog.h"

#include "SkyChartExporter.hpp"
#include "SkyChartRenderer.hpp"
#include "PreviewDialog.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelMainView.hpp"
#include <QTimer>
#include <QFontDatabase>
#include <QDebug>
#include <QEvent>
#include <QLayout>

#include "StarMgr.hpp"
#include "ConstellationMgr.hpp"
#include "StelSkyCultureMgr.hpp"
#include "StelFileMgr.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QListWidgetItem>

//////////////////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////////////////

SkyChartExporterDialog::SkyChartExporterDialog()
    : StelDialog("SkyChartExporter")
    , ui(nullptr)
    , exporter(nullptr)
{
}

//////////////////////////////////////////////////////////////////
// Destructor
//////////////////////////////////////////////////////////////////

SkyChartExporterDialog::~SkyChartExporterDialog()
{
    delete ui;
    delete previewDialog;
}

//////////////////////////////////////////////////////////////////
// Visible
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::setVisible(bool visible)
{
    StelDialog::setVisible(visible);
}

//////////////////////////////////////////////////////////////////
// Traducción
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::retranslate()
{
    if (ui && dialog)
        ui->retranslateUi(dialog);
}

//////////////////////////////////////////////////////////////////
// Creación del diálogo
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::createDialogContent()
{
    ui = new Ui::SkyChartExporterDialog;
    ui->setupUi(qobject_cast<QWidget*>(dialog));

    // ==========================================================
    // Configuración y conexión de la barra de título personalizada
    // ==========================================================
    if (ui->titleBar)
    {
        ui->titleBar->setTitle(qobject_cast<QWidget*>(dialog)->windowTitle());
        connect(ui->titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
        connect(ui->titleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));
    }

    // Inicializar UI primero, independiente del módulo
    populateCombos();
    loadDefaults();
    connectSignals();
    setStatus(tr("Listo"));

    // Obtener módulo (opcional para la UI)
    exporter = qobject_cast<SkyChartExporter*>(
        StelApp::getInstance()
            .getModuleMgr()
            .getModule("SkyChartExporter"));

    if (!exporter)
        qWarning() << "[SkyChartExporter] módulo no encontrado.";

    // Falla 2: escuchar los resize de la ventana principal de Stellarium para
    // poder reacomodar este diálogo si queda fuera del área visible.
    StelMainView::getInstance().installEventFilter(this);

    // ==========================================================
    // StelDialog restaura el tamaño
    // guardado de una sesión anterior en config.ini (ver
    // StelDialog::handleDialogSizeChanged), y ese resize() posterior pisa el
    // sizeHint calculado por el layout del .ui, dejando espacio muerto.
    // Se fuerza aquí, al final de createDialogContent() y ya con todos los
    // combos/labels poblados (para que el sizeHint sea el real), a que el
    // diálogo quede fijo en su tamaño natural de contenido. setFixedSize()
    // fija mínimo=máximo=natural, así cualquier resize() posterior -incluido
    // uno que intente restaurar un tamaño viejo más grande- queda acotado
    // por Qt en vez de volver a inflar el diálogo.
    // ==========================================================
    if (QWidget* w = qobject_cast<QWidget*>(dialog))
    {
        if (w->layout())
            w->layout()->activate();
        w->adjustSize();
        
        // Obtenemos el tamaño natural calculado por los elementos de la interfaz
        QSize natural = w->sizeHint();
        
        // Forzamos una proporción vertical adecuada (más alto que ancho) 
        // garantizando un ancho mínimo cómodo y una altura suficiente para 
        // que todas las pestañas y los botones inferiores queden completamente visibles.
        int forcedWidth  = qMax(natural.width(), 350);
        int forcedHeight = qMax(natural.height(), 600);
        QSize properSize(forcedWidth, forcedHeight);
        
        w->setFixedSize(properSize);
        if (proxy)
            proxy->resize(properSize);
    }
}

//////////////////////////////////////////////////////////////////
// Falla 2: reacomodo del diálogo al redimensionar la ventana principal
//////////////////////////////////////////////////////////////////

bool SkyChartExporterDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == &StelMainView::getInstance() && event->type() == QEvent::Resize
        && dialog && proxy)
    {
        // "dialog" es el QWidget con el contenido; "proxy" es el
        // QGraphicsProxyWidget que lo ubica dentro de la escena de
        // StelMainView (miembros protegidos de StelDialog). La posición que
        // hay que reacomodar es la del proxy, no la del QWidget en sí.
        const QSizeF viewSize = StelMainView::getInstance().size();
        const QSizeF dlgSize  = dialog->size();

        const double maxX = qMax(0.0, viewSize.width()  - dlgSize.width());
        const double maxY = qMax(0.0, viewSize.height() - dlgSize.height());

        const QPointF pos = proxy->pos();
        const QPointF clamped(qBound(0.0, pos.x(), maxX),
                               qBound(0.0, pos.y(), maxY));

        if (clamped != pos)
            proxy->setPos(clamped);
    }

    return QObject::eventFilter(watched, event);
}

//////////////////////////////////////////////////////////////////
// Obtener módulo en tiempo de ejecución
//////////////////////////////////////////////////////////////////
SkyChartExporter* SkyChartExporterDialog::getExporter() const
{
    return qobject_cast<SkyChartExporter*>(
        StelApp::getInstance()
            .getModuleMgr()
            .getModule("SkyChartExporter"));
}

//////////////////////////////////////////////////////////////////
// Inicializar listas
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::populateCombos()
{
    // Papel y Formato
    ui->comboPaperSize->clear();
    ui->comboPaperSize->addItems({"A4", "A3", "A2", "Carta", "Legal"});
    
    
    // Orientación
    ui->comboOrientation->clear();
    ui->comboOrientation->addItems({tr("Vertical"), tr("Horizontal")});

    // Escala real de tamaños de objetos
    ui->comboScale->clear();
    ui->comboScale->addItems({"0.5", "1.0", "1.5", "2.0", "3.0"});


    // Selector de Etiquetas Generales y Específicas (Magnitudes y Ninguno)
    ui->comboStarLabelMagnitude->clear();
    ui->comboTargetLabelMagnitude->clear();

    ui->comboStarLabelMagnitude->addItem(tr("Ninguno"), -99.0);
    ui->comboTargetLabelMagnitude->addItem(tr("Ninguno"), -99.0);

    for (double m = -2.0; m <= 6.5; m += 0.5)
    {
        QString labelText = QString::number(m, 'f', 1);
        ui->comboStarLabelMagnitude->addItem(labelText, m);
        ui->comboTargetLabelMagnitude->addItem(labelText, m);
    }


    // Modo de color unificado (Color, Escala de grises, Monocromo)
    ui->comboPrintColor->clear();
    ui->comboPrintColor->addItems({tr("Color"), tr("Escala de grises"), tr("Monocromo")});
    
    ui->comboDpi->clear();
    ui->comboDpi->addItems({"72", "150", "300", "600", "1200"});
    
    

    // Fuentes disponibles
    QFontDatabase db;
    QStringList fonts = db.families();

    ui->comboTitleFont->addItems(fonts);
    ui->comboSubtitleFont->addItems(fonts);
    ui->comboStarLabelFont->addItems(fonts);
    ui->comboTargetFont->addItems(fonts); // Fuente para Astros Específicos

    // Tamaños
    QStringList sizes = {
        "8","9","10","11","12",
        "14","16","18","20","24",
        "28","32","36"
    };

    ui->comboTitleSize->addItems(sizes);
    ui->comboSubtitleSize->addItems(sizes);
    ui->comboStarLabelSize->addItems(sizes);
    ui->comboTargetSize->addItems(sizes); // Tamaño para Astros Específicos
}

//////////////////////////////////////////////////////////////////
// Valores iniciales
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::loadDefaults()
{
    // Papel y escala
    ui->comboPaperSize->setCurrentText("A4");
    ui->comboOrientation->setCurrentIndex(0);
    ui->comboScale->setCurrentText("1.0");
    
    
    // Configuración de Color e Impresión
    ui->comboPrintColor->setCurrentIndex(0);
    ui->comboDpi->setCurrentText("300");
    

    // Márgenes por defecto
    applyDefaultMargins();

    // Estrellas y Etiquetas
    ui->spinLimitingMagnitude->setValue(6.5);
    int idxStarMag = ui->comboStarLabelMagnitude->findData(3.0);
    if (idxStarMag != -1)
        ui->comboStarLabelMagnitude->setCurrentIndex(idxStarMag);

    ui->comboStarLabelFont->setCurrentText("Times New Roman");
    ui->comboStarLabelSize->setCurrentText("9");
    ui->buttonStarLabelBold->setChecked(false);
    ui->buttonStarLabelItalic->setChecked(false);
    
    // Astro Específico
    
    int idxTargetMag = ui->comboTargetLabelMagnitude->findData(2.0);
    if (idxTargetMag != -1)
        ui->comboTargetLabelMagnitude->setCurrentIndex(idxTargetMag);
    ui->comboTargetFont->setCurrentText("Times New Roman");
    ui->comboTargetSize->setCurrentText("12");
    ui->buttonTargetBold->setChecked(true);
    ui->buttonTargetItalic->setChecked(false);

    // Elementos gráficos
    ui->checkNorthArrow->setChecked(true);
    ui->checkMagnitudeLegend->setChecked(true);
    ui->checkSpectralLegend->setChecked(true);
    ui->checkShowHorizon->setChecked(true);
    ui->checkShowMilkyWay->setChecked(true);
    ui->checkShowAstroLegend->setChecked(true);
    ui->sliderMilkyWayAlpha->setRange(0, 255);
    ui->sliderMilkyWayAlpha->setValue(150);

    // Información
    ui->editTitle->clear();
    ui->editSubtitle->clear();

    ui->comboTitleFont->setCurrentText("Times New Roman");
    ui->comboSubtitleFont->setCurrentText("Times New Roman");

    ui->comboTitleSize->setCurrentText("24");
    ui->comboSubtitleSize->setCurrentText("14");

    ui->buttonTitleBold->setChecked(true);
    ui->buttonTitleItalic->setChecked(false);
    ui->buttonSubtitleBold->setChecked(false);
    ui->buttonSubtitleItalic->setChecked(false);

    // Limpiar lista visual de astros específicos para sincronizar con la memoria
    if (ui->listWidgetSpecificTargets)
        ui->listWidgetSpecificTargets->clear();

    options = SkyChartExportOptions();
    
    // Forzar visualización por defecto en modo PDF
    setFormatToPDF();
    
    updateOptions();
}

//////////////////////////////////////////////////////////////////
// Conexión de señales
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::connectSignals()
{
    // Alternancia Dinámica (PDF vs Imagen)
    connect(ui->buttonFormatPDF, &QPushButton::clicked, this, &SkyChartExporterDialog::setFormatToPDF);
    connect(ui->buttonFormatImage, &QPushButton::clicked, this, &SkyChartExporterDialog::setFormatToImage);

    // Papel y Escala
    connect(ui->comboPaperSize, &QComboBox::currentTextChanged, this, [this](const QString& text){
        if (ui->buttonFormatImage->isChecked()) {
            bool isCustom = (text == tr("Personalizado"));
            ui->spinCustomWidth->setEnabled(isCustom);
            ui->spinCustomHeight->setEnabled(isCustom);
        }
        updateOptions();
    });
    connect(ui->spinCustomWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->spinCustomHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboOrientation, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboScale, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    

    // Impresión Raster y Calidad
    connect(ui->comboPrintColor, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboDpi, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->lineWeight, &QLineEdit::textChanged, this, &SkyChartExporterDialog::updateOptions); // <-- NUEVA 
    
    // Márgenes
    connect(ui->spinMarginLeft, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->spinMarginRight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->spinMarginTop, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->spinMarginBottom, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);

    connect(ui->buttonNoMargins, &QPushButton::clicked, this, &SkyChartExporterDialog::applyNoMargins);
    connect(ui->buttonDefaultMargins, &QPushButton::clicked, this, &SkyChartExporterDialog::applyDefaultMargins);

    // Estrellas y Etiquetas
    connect(ui->spinLimitingMagnitude, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboStarLabelMagnitude, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboStarLabelFont, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboStarLabelSize, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->buttonStarLabelBold, &QToolButton::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->buttonStarLabelItalic, &QToolButton::toggled, this, &SkyChartExporterDialog::updateOptions);

    // Elementos gráficos
    connect(ui->checkNorthArrow, &QCheckBox::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->checkMagnitudeLegend, &QCheckBox::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->checkSpectralLegend, &QCheckBox::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->checkShowHorizon, &QCheckBox::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->checkShowMilkyWay, &QCheckBox::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->checkShowAstroLegend, &QCheckBox::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->sliderMilkyWayAlpha, &QSlider::valueChanged, this, &SkyChartExporterDialog::updateOptions);

    // Texto de Título y Subtítulo
    connect(ui->editTitle, &QLineEdit::textChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->editSubtitle, &QLineEdit::textChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboTitleFont, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboSubtitleFont, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboTitleSize, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->comboSubtitleSize, &QComboBox::currentTextChanged, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->buttonTitleBold, &QToolButton::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->buttonTitleItalic, &QToolButton::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->buttonSubtitleBold, &QToolButton::toggled, this, &SkyChartExporterDialog::updateOptions);
    connect(ui->buttonSubtitleItalic, &QToolButton::toggled, this, &SkyChartExporterDialog::updateOptions);

    // Botones Principales
    connect(ui->buttonPreview, &QPushButton::clicked, this, &SkyChartExporterDialog::updatePreview);
    connect(ui->buttonExport, &QPushButton::clicked, this, &SkyChartExporterDialog::exportPdf);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &StelDialog::close);

    // Conectar botones de gestión de objetivos específicos
    connect(ui->buttonAddSpecificTarget, &QPushButton::clicked, this, &SkyChartExporterDialog::addSelectedObjectFromStellarium);
    connect(ui->buttonRemoveSpecificTarget, &QPushButton::clicked, this, &SkyChartExporterDialog::removeSelectedTargetFromList);
    connect(ui->buttonClearSpecificTargets, &QPushButton::clicked, this, &SkyChartExporterDialog::clearAllSpecificTargets);

// Permitir seleccionar varios astros de la lista a la vez, útil para
// eliminarlos en bloque con removeSelectedTargetFromList(). Ya NO se usa
// para decidir a quién se le aplica el formato (ver rediseño más abajo).
    ui->listWidgetSpecificTargets->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Sincroniza los controles de formato con el valor compartido de la
    // lista (todos los astros de la lista usan el mismo formato).
    connect(ui->listWidgetSpecificTargets, &QListWidget::itemSelectionChanged,
            this, &SkyChartExporterDialog::loadSelectedTargetFormat);

    // Estos controles ya NO aplican el formato solo al ítem seleccionado:
    // se propagan a TODOS los astros de la lista (options.specificTargets).
    // La selección de un objeto en el cielo de Stellarium, y la selección
    // en esta lista, solo sirven para agregar/eliminar astros de la lista.
    connect(ui->comboTargetLabelMagnitude, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SkyChartExporterDialog::updateSelectedTargetFormat);


    connect(ui->comboTargetFont, &QComboBox::currentTextChanged,
            this, &SkyChartExporterDialog::updateSelectedTargetFormat);
    connect(ui->comboTargetSize, &QComboBox::currentTextChanged,
            this, &SkyChartExporterDialog::updateSelectedTargetFormat);
    connect(ui->buttonTargetBold, &QPushButton::toggled,
            this, &SkyChartExporterDialog::updateSelectedTargetFormat);
    connect(ui->buttonTargetItalic, &QPushButton::toggled,
            this, &SkyChartExporterDialog::updateSelectedTargetFormat);
    
}


//////////////////////////////////////////////////////////////////
// Captura de astros
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::addSelectedObjectFromStellarium()
{
    StelObjectMgr* objMgr = GETSTELMODULE(StelObjectMgr);
    if (!objMgr) return;

    QList<StelObjectP> selectedObjects = objMgr->getSelectedObject();
    if (selectedObjects.isEmpty())
    {
        setStatus(tr("Advertencia: Ningún objeto seleccionado en la pantalla de Stellarium."));
        return;
    }

    StelObjectP obj = selectedObjects.first();

    // ¿El objeto seleccionado es una estrella con HIP resoluble?
    static const QRegularExpression hipRe(QStringLiteral("HIP\\s*(\\d+)"));
    const QRegularExpressionMatch match = hipRe.match(obj->getID());
    const bool isStar = match.hasMatch();
    const int hip = isStar ? match.captured(1).toInt() : 0;

    // Si es estrella Y las líneas de constelación están activas en Stellarium,
    // se agrega el grupo completo de estrellas de esa constelación
    auto* constMgr = GETSTELMODULE(ConstellationMgr);
    const bool groupByConstellation = isStar && hip > 0 && constMgr && constMgr->getFlagLines();

    QList<int> hipsToAdd;
    if (groupByConstellation)
    {
        hipsToAdd = getConstellationHipsForStar(hip);
        if (hipsToAdd.isEmpty())
            hipsToAdd.append(hip); // no se encontró su constelación: agregar solo esa estrella
    }
    else if (isStar)
    {
        hipsToAdd.append(hip);
    }

    int addedCount = 0;
    QStringList addedNames;

	ui->listWidgetSpecificTargets->clearSelection();

    if (!hipsToAdd.isEmpty())
    {
        // ---- Camino de ESTRELLAS (individual o grupo de constelación) ----
        for (int h : hipsToAdd)
        {
            SpecificAstroRule rule;
            rule.id = QString("HIP %1").arg(h);

            QString name = StarMgr::getCommonNameI18n(h);
            if (name.isEmpty())
                name = StarMgr::getSciDesignation(h);
            if (name.isEmpty())
                name = rule.id;

            // Igual que en resolveStarLabel() del extractor: si la designación
            // viene compuesta ("β Ori - 19 Ori"), en la lista solo mostramos
            // la primera parte. rule.id NO se toca (sigue siendo "HIP h"),
            // así que el matching con el extractor/renderer no se ve afectado.
            const int dashIdx = name.indexOf(QStringLiteral(" - "));
            if (dashIdx > 0)
                name = name.left(dashIdx).trimmed();

            rule.displayName = name;

            // NUEVO: sembrar el formato inicial con lo que esté puesto AHORA MISMO
            // en los controles de la sección Específica, en vez de dejarlo con los
            // valores de fábrica de SpecificAstroRule (Noto Sans, -1, sin negrita/
            // cursiva, mag 6.5). Así, si configuras el formato antes de "Agregar",
            // ese formato ya viene aplicado desde el primer momento.
            QVariant targetMagData = ui->comboTargetLabelMagnitude->currentData();
            rule.limitingMagnitude = targetMagData.isValid() ? targetMagData.toDouble() : 2.0;
            rule.bold = ui->buttonTargetBold->isChecked();
            rule.italic = ui->buttonTargetItalic->isChecked();
            rule.font = QFont(ui->comboTargetFont->currentText(), ui->comboTargetSize->currentText().toInt());
            rule.font.setBold(rule.bold);
            rule.font.setItalic(rule.italic);

            bool duplicate = false;
            for (const auto& existing : options.specificTargets)
            {
                if (existing.id == rule.id) { duplicate = true; break; }
            }
            if (duplicate) continue;

            options.specificTargets.append(rule);

            QListWidgetItem* item = new QListWidgetItem(rule.displayName);
            item->setData(Qt::UserRole, rule.id);

           ui->listWidgetSpecificTargets->addItem(item);
            item->setSelected(true);   // NUEVO: deja el astro recién agregado listo para editar su formato

            ++addedCount;
            addedNames << rule.displayName;
        }
    }
    else
    {
        // ---- Camino ORIGINAL: cualquier otro astro (planeta, nebulosa, Sol, Luna...) ----
       SpecificAstroRule rule;
        rule.id = obj->getID();
        rule.displayName = obj->getNameI18n().isEmpty() ? obj->getEnglishName() : obj->getNameI18n();

        // NUEVO: mismo sembrado de formato inicial que en la rama de estrellas
        QVariant targetMagData = ui->comboTargetLabelMagnitude->currentData();
    rule.limitingMagnitude = targetMagData.isValid() ? targetMagData.toDouble() : 2.0;
        rule.bold = ui->buttonTargetBold->isChecked();
        rule.italic = ui->buttonTargetItalic->isChecked();
        rule.font = QFont(ui->comboTargetFont->currentText(), ui->comboTargetSize->currentText().toInt());
        rule.font.setBold(rule.bold);
        rule.font.setItalic(rule.italic);

        for (const auto& existing : options.specificTargets)
        {
            if (existing.id == rule.id)
            {
                setStatus(tr("El astro '%1' ya se encuentra en la lista de objetivos.").arg(rule.displayName));
                return;
            }
        }

        options.specificTargets.append(rule);

        QListWidgetItem* item = new QListWidgetItem(rule.displayName);
        item->setData(Qt::UserRole, rule.id);


       ui->listWidgetSpecificTargets->addItem(item);
        item->setSelected(true);   // NUEVO

        addedCount = 1;
        addedNames << rule.displayName;
    }

    if (addedCount == 0)
    {
        setStatus(tr("El astro seleccionado ya se encuentra en la lista de objetivos."));
        return;
    }

    updateOptions();

    if (groupByConstellation && addedCount > 1)
        setStatus(tr("Constelación agregada: %1 estrellas añadidas.").arg(addedCount));
    else
        setStatus(tr("Astro agregado con éxito: %1").arg(addedNames.join(", ")));
}


void SkyChartExporterDialog::removeSelectedTargetFromList()
{
    QList<QListWidgetItem*> selectedItems = ui->listWidgetSpecificTargets->selectedItems();

    if (selectedItems.isEmpty())
    {
        setStatus(tr("Selecciona un objetivo en la lista para eliminarlo."));
        return;
    }

    int removedCount = 0;
    for (QListWidgetItem* item : selectedItems)
    {
        QString id = item->data(Qt::UserRole).toString();

        // Eliminar del modelo interno de opciones
        for (int i = 0; i < options.specificTargets.size(); ++i)
        {
            if (options.specificTargets.at(i).id == id)
            {
                options.specificTargets.removeAt(i);
                break;
            }
        }

        // Eliminar del control visual de la UI
        delete ui->listWidgetSpecificTargets->takeItem(ui->listWidgetSpecificTargets->row(item));
        ++removedCount;
    }

    setStatus(tr("Se eliminaron %1 objetivos de la lista.").arg(removedCount));
    updateOptions();
}


void SkyChartExporterDialog::clearAllSpecificTargets()
{
    if (options.specificTargets.isEmpty() && ui->listWidgetSpecificTargets->count() == 0)
    {
        setStatus(tr("La lista de objetivos específicos ya está vacía."));
        return;
    }

    // Vaciar el modelo de datos interno
    options.specificTargets.clear();

    // Vaciar el componente visual de la UI
    ui->listWidgetSpecificTargets->clear();

    // Actualizar opciones generales y notificar al usuario
    updateOptions();
    setStatus(tr("Se ha limpiado toda la lista de objetivos específicos."));
}

void SkyChartExporterDialog::loadSelectedTargetFormat()
{
    // REDISEÑO: como el formato es único para TODA la lista (ver
    // updateSelectedTargetFormat), ya no hace falta mirar qué ítem está
    // seleccionado: cualquier regla de options.specificTargets sirve como
    // referencia, porque todas tienen los mismos valores. Se usa la primera
    // simplemente para sincronizar los controles con lo que ya está guardado
    // (por ejemplo, al reabrir el diálogo o al seleccionar en la lista).
    if (options.specificTargets.isEmpty())
        return;

    const SpecificAstroRule& rule = options.specificTargets.first();

    // Bloquear señales para no re-disparar updateSelectedTargetFormat() en cascada
    ui->comboTargetLabelMagnitude->blockSignals(true);
    ui->comboTargetFont->blockSignals(true);
    ui->comboTargetSize->blockSignals(true);
    ui->buttonTargetBold->blockSignals(true);
    ui->buttonTargetItalic->blockSignals(true);

    int idxTargetMag = ui->comboTargetLabelMagnitude->findData(rule.limitingMagnitude);
    if (idxTargetMag != -1)
        ui->comboTargetLabelMagnitude->setCurrentIndex(idxTargetMag);
    ui->comboTargetFont->setCurrentText(rule.font.family());
    ui->comboTargetSize->setCurrentText(QString::number(rule.font.pointSize() > 0 ? rule.font.pointSize() : 12));
    ui->buttonTargetBold->setChecked(rule.bold);
    ui->buttonTargetItalic->setChecked(rule.italic);

    ui->comboTargetLabelMagnitude->blockSignals(false);
    ui->comboTargetFont->blockSignals(false);
    ui->comboTargetSize->blockSignals(false);
    ui->buttonTargetBold->blockSignals(false);
    ui->buttonTargetItalic->blockSignals(false);
}

void SkyChartExporterDialog::updateSelectedTargetFormat()
{
    // REDISEÑO: el formato ya NO se aplica solo al/los ítem(s) seleccionado(s)
    // en la lista. La lista "Astro" (agregar/eliminar/limpiar) es la única
    // fuente de verdad: cualquier astro que esté en options.specificTargets
    // recibe el mismo formato. La selección en listWidgetSpecificTargets solo
    // se usa para eliminar elementos puntuales (removeSelectedTargetFromList);
    // y la selección de un objeto en el cielo de Stellarium solo sirve para
    // agregarlo (addSelectedObjectFromStellarium). Ninguna de las dos
    // selecciones determina a quién se le aplica el formato.
    if (options.specificTargets.isEmpty())
        return;

    QVariant targetMagData = ui->comboTargetLabelMagnitude->currentData();
    const double mag = targetMagData.isValid() ? targetMagData.toDouble() : 2.0;
    const QString font  = ui->comboTargetFont->currentText();
    const int size       = ui->comboTargetSize->currentText().toInt();
    const bool bold      = ui->buttonTargetBold->isChecked();
    const bool italic    = ui->buttonTargetItalic->isChecked();

    for (auto& rule : options.specificTargets)
    {
        rule.limitingMagnitude = mag;
        rule.font = QFont(font, size);
        rule.font.setBold(bold);
        rule.font.setItalic(italic);
        rule.bold = bold;
        rule.italic = italic;

        qDebug() << "[DEBUG-1] Escrito -> id=" << rule.id << "font=" << rule.font.family()
         << rule.font.pointSize() << "bold=" << rule.bold << "italic=" << rule.italic;
    }

    updateOptions();
}

//////////////////////////////////////////////////////////////////
// Selecionar contelacion
//////////////////////////////////////////////////////////////////

QList<int> SkyChartExporterDialog::getConstellationHipsForStar(int hip) const
{
    QList<int> result;

    auto* skyCultureMgr = GETSTELMODULE(StelSkyCultureMgr);
    if (!skyCultureMgr) return result;

    const QString cultureId = skyCultureMgr->getCurrentSkyCultureID();
    if (cultureId.isEmpty()) return result;

    const QString indexPath = StelFileMgr::findFile(
        QString("skycultures/%1/index.json").arg(cultureId));
    if (indexPath.isEmpty()) return result;

    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly)) return result;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return result;

    const QJsonArray constellationsArray = doc.object().value("constellations").toArray();

    for (const QJsonValue& consVal : constellationsArray)
    {
        const QJsonObject consObj = consVal.toObject();
        const QJsonArray linesArray = consObj.value("lines").toArray();

        QSet<int> hipsInThisConstellation;
        bool containsTarget = false;

        for (const QJsonValue& segmentVal : linesArray)
        {
            const QJsonArray hipChain = segmentVal.toArray();
            for (const QJsonValue& hipVal : hipChain)
            {
                const int h = hipVal.toInt(-1);
                if (h < 0) continue;

                hipsInThisConstellation.insert(h);
                if (h == hip) containsTarget = true;
            }
        }

        if (containsTarget)
        {
            result = hipsInThisConstellation.values();
            break;
        }
    }

    return result;
}

//////////////////////////////////////////////////////////////////
// Lógica Dinámica de UI
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::setFormatToPDF()
{
    ui->buttonFormatPDF->setChecked(true);
    ui->buttonFormatImage->setChecked(false);

    ui->groupBox_2->setEnabled(true);    
    ui->groupPaper->setEnabled(true);
    ui->groupPaper->setTitle(tr("Configuración del papel"));
    ui->groupMargins->setEnabled(true);
    ui->groupMargins->setTitle(tr("Márgenes (mm)"));
    
    // Listas dinámicas para PDF
    ui->comboPaperSize->clear();
    ui->comboPaperSize->addItems({"A4", "A3", "A2", "Carta", "Legal"});
   
    // Mostrar orientación del PDF y ocultar controles numéricos de imagen
    ui->labelOrientation->show();
    ui->comboOrientation->show();
    ui->labelCustomWidth->hide();
    ui->spinCustomWidth->hide();
    ui->labelCustomHeight->hide();
    ui->spinCustomHeight->hide();

    ui->comboPrintColor->clear();
    ui->comboPrintColor->addItems({tr("Color"), tr("Escala de grises"), tr("Monocromo")});
    
    // Formato de márgenes en milímetros
    ui->spinMarginLeft->setDecimals(1);
    ui->spinMarginRight->setDecimals(1);
    ui->spinMarginTop->setDecimals(1);
    ui->spinMarginBottom->setDecimals(1);

    ui->spinMarginLeft->setSuffix(" mm");
    ui->spinMarginRight->setSuffix(" mm");
    ui->spinMarginTop->setSuffix(" mm");
    ui->spinMarginBottom->setSuffix(" mm");

    // PESO EN PDF: Solo lectura (Calculado en MB)
    ui->lineWeight->setReadOnly(true);
    ui->lineWeight->setToolTip(tr("Peso estimado del documento en MB"));

    // LIMPIEZA: Aplicar márgenes predefinidos de PDF (mm) y sincronizar
    applyDefaultMargins();
    updateOptions();

    setStatus(tr("Modo Vectorial (PDF) Activado"));
}


void SkyChartExporterDialog::setFormatToImage()
{
    ui->buttonFormatImage->setChecked(true);
    ui->buttonFormatPDF->setChecked(false);

    ui->groupBox_2->setEnabled(true);
    ui->groupPaper->setEnabled(true);
    ui->groupPaper->setTitle(tr("Configuración del lienzo"));
    ui->groupMargins->setEnabled(true);
    ui->groupMargins->setTitle(tr("Márgenes (Pixeles)"));
    
    // Listas dinámicas para IMAGEN
    ui->comboPaperSize->clear();
    ui->comboPaperSize->addItems({
        tr("Personalizado"), 
        tr("1920x1080 (Full HD)"), 
        tr("2560x1440 (2K)"), 
        tr("3840x2160 (4K)"), 
        tr("1024x1024 (Cuadrado)")
    });

    // Ocultar orientación del PDF y mostrar los controles de ancho/alto
    ui->labelOrientation->hide();
    ui->comboOrientation->hide();
    ui->labelCustomWidth->show();
    ui->spinCustomWidth->show();
    ui->labelCustomHeight->show();
    ui->spinCustomHeight->show();

    // Habilitar campos numéricos SÓLO si la opción seleccionada es "Personalizado"
    bool isCustom = (ui->comboPaperSize->currentText() == tr("Personalizado"));
    ui->spinCustomWidth->setEnabled(isCustom);
    ui->spinCustomHeight->setEnabled(isCustom);

    ui->comboPrintColor->clear();
    ui->comboPrintColor->addItems({tr("Color"), tr("Escala de grises"), tr("Monocromo")});
    
    // Formato de márgenes en Píxeles enteros
    ui->spinMarginLeft->setDecimals(0);
    ui->spinMarginRight->setDecimals(0);
    ui->spinMarginTop->setDecimals(0);
    ui->spinMarginBottom->setDecimals(0);

    ui->spinMarginLeft->setSuffix(" px");
    ui->spinMarginRight->setSuffix(" px");
    ui->spinMarginTop->setSuffix(" px");
    ui->spinMarginBottom->setSuffix(" px");

    // PESO EN IMAGEN: Editable por el usuario (Calidad 1 a 100)
    ui->lineWeight->setReadOnly(false);
    ui->lineWeight->setText("92"); // Calidad JPEG por defecto
    ui->lineWeight->setToolTip(tr("Ingrese la calidad de imagen de 1 a 100%"));

    // LIMPIEZA: Aplicar márgenes predefinidos de Imagen (px) y sincronizar
    applyDefaultMargins();
    updateOptions();

    setStatus(tr("Modo Gráfico (Imagen) Activado"));
}

//////////////////////////////////////////////////////////////////
// Leer controles
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::updateOptions()
{
    // Papel y dimensiones en píxeles
    QString paperSelection = ui->comboPaperSize->currentText();
    if (ui->buttonFormatImage->isChecked()) {
        if (paperSelection == tr("Personalizado")) {
            // Extraer ancho y alto interactivo definido por el usuario
            options.imageWidthPx = ui->spinCustomWidth->value();
            options.imageHeightPx = ui->spinCustomHeight->value();
        } else if (paperSelection.contains("x")) {
            // Limpiar la cadena (ej. separar "1920x1080" de "(Full HD)")
            QString dims = paperSelection.split(" ").first();
            QStringList parts = dims.split("x");
            if (parts.size() >= 2) {
                options.imageWidthPx = parts[0].toInt();
                options.imageHeightPx = parts[1].toInt();
            }
        }
        options.paperSize = "A4"; // Fallback interno para referenciar el layout base
    } else {
        options.imageWidthPx = 0;
        options.imageHeightPx = 0;
        options.paperSize = paperSelection;
    }

    if (ui->buttonFormatImage->isChecked() && options.imageWidthPx > 0 && options.imageHeightPx > 0) {
        options.orientation = (options.imageWidthPx > options.imageHeightPx) ? "Horizontal" : "Vertical";
    } else {
        options.orientation = ui->comboOrientation->currentText();
    }
    
    // Escala real de tamaños
    options.scale = ui->comboScale->currentText().toDouble();
    if (options.scale <= 0.0) options.scale = 1.0;
    options.planetScale = options.scale;

    // Modo de color (Traducción e interpretación unificada para el renderizador)
    QString rawColorText = ui->comboPrintColor->currentText().toLower();

    if (rawColorText.contains("monocromo") || rawColorText.contains("monochrome"))
    {
        options.colorMode = "Monocromo";
        options.grayscale = false;
    }
    else if (rawColorText.contains("grises") || rawColorText.contains("grayscale"))
    {
        options.colorMode = "Escala de grises";
        options.grayscale = true;
    }
    else
    {
        options.colorMode = "Color";
        options.grayscale = false;
    }
    
    // DPI
    options.dpi = ui->comboDpi->currentText().toInt();
    if (options.dpi <= 0) options.dpi = 300;

    // --- CONVERSIÓN INTELIGENTE DE MÁRGENES ---
    if (ui->buttonFormatImage->isChecked()) {
        // Convertimos matemáticamente los píxeles de la UI a milímetros internos
        double dpiFactor = static_cast<double>(options.dpi);
        options.marginLeft   = (ui->spinMarginLeft->value() / dpiFactor) * 25.4;
        options.marginRight  = (ui->spinMarginRight->value() / dpiFactor) * 25.4;
        options.marginTop    = (ui->spinMarginTop->value() / dpiFactor) * 25.4;
        options.marginBottom = (ui->spinMarginBottom->value() / dpiFactor) * 25.4;
    } else {
        // Ya están en milímetros nativos
        options.marginLeft = ui->spinMarginLeft->value();
        options.marginRight = ui->spinMarginRight->value();
        options.marginTop = ui->spinMarginTop->value();
        options.marginBottom = ui->spinMarginBottom->value();
    }

    // Estrellas y Etiquetas
    options.limitingMagnitude = ui->spinLimitingMagnitude->value();
    QVariant starMagData = ui->comboStarLabelMagnitude->currentData();
    options.starLabelMagnitude = starMagData.isValid() ? starMagData.toDouble() : 3.0;

    options.starLabelFont = QFont(ui->comboStarLabelFont->currentText(),
                                  ui->comboStarLabelSize->currentText().toInt());
    options.starLabelFont.setBold(ui->buttonStarLabelBold->isChecked());
    options.starLabelFont.setItalic(ui->buttonStarLabelItalic->isChecked());

    // Elementos gráficos
    options.showNorthArrow = ui->checkNorthArrow->isChecked();
    options.showMagnitudeLegend = ui->checkMagnitudeLegend->isChecked();
    options.showSpectralLegend = ui->checkSpectralLegend->isChecked();
    options.showHorizon = ui->checkShowHorizon->isChecked();
    options.showMilkyWay = ui->checkShowMilkyWay->isChecked();
    options.showAstroLegend = ui->checkShowAstroLegend->isChecked();
    options.milkyWayAlpha = ui->sliderMilkyWayAlpha->value();

    // Información
    options.title = ui->editTitle->text();
    options.subtitle = ui->editSubtitle->text();

    options.titleFont = QFont(ui->comboTitleFont->currentText(),
                              ui->comboTitleSize->currentText().toInt());
    options.titleFont.setBold(ui->buttonTitleBold->isChecked());
    options.titleFont.setItalic(ui->buttonTitleItalic->isChecked());

    options.subtitleFont = QFont(ui->comboSubtitleFont->currentText(),
                                 ui->comboSubtitleSize->currentText().toInt());
    options.subtitleFont.setBold(ui->buttonSubtitleBold->isChecked());
    options.subtitleFont.setItalic(ui->buttonSubtitleItalic->isChecked());

 // LÓGICA DE PESO Y CALIDAD DINÁMICA
    if (ui->buttonFormatImage->isChecked())
    {
        // MODO IMAGEN: Extraer solo los dígitos numéricos (Qt 6)
        QString rawText = ui->lineWeight->text();
        QString cleanText;
        for (const QChar &ch : rawText) {
            if (ch.isDigit()) {
                cleanText.append(ch);
            }
        }
        int qualityVal = cleanText.toInt();
        
        if (qualityVal <= 0 || qualityVal > 100) 
            qualityVal = 92; // Valor seguro si el campo está vacío o fuera de rango

        options.imageQuality = qualityVal;
        options.exportFormat = "PNG";
    }
    else
    {
        // MODO PDF: Estimación matemática automática del peso en MB
        double scaleVal = ui->comboScale->currentText().toDouble();
        if (scaleVal <= 0.0) scaleVal = 1.0;

        int dpiVal = ui->comboDpi->currentText().toInt();
        if (dpiVal <= 0) dpiVal = 300;

        bool isColorMode = (ui->comboPrintColor->currentIndex() == 0);

        double baseWeight = 0.4; 
        double calculatedWeight = baseWeight * scaleVal * (static_cast<double>(dpiVal) / 300.0) * (isColorMode ? 1.3 : 1.0);

        // Bloquear señales momentáneamente para no generar bucles infinitos al escribir el texto
        ui->lineWeight->blockSignals(true);
        ui->lineWeight->setText(QString::number(calculatedWeight, 'f', 2) + " MB");
        ui->lineWeight->blockSignals(false);
	options.exportFormat = "PDF";
    }
}

//////////////////////////////////////////////////////////////////
// SIN MARGENES
//////////////////////////////////////////////////////////////////


void SkyChartExporterDialog::applyNoMargins()
{
    ui->spinMarginLeft->setValue(0.0);
    ui->spinMarginRight->setValue(0.0);
    ui->spinMarginTop->setValue(0.0);
    ui->spinMarginBottom->setValue(0.0);
}

//////////////////////////////////////////////////////////////////
// MARGENES PREFIJADOS
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::applyDefaultMargins()
{
    // Desconectamos temporalmente las señales para que los 4 cambios no saturen la UI
    ui->spinMarginLeft->blockSignals(true);
    ui->spinMarginRight->blockSignals(true);
    ui->spinMarginTop->blockSignals(true);
    ui->spinMarginBottom->blockSignals(true);

    if (ui->buttonFormatImage->isChecked())
    {
        // Márgenes predefinidos NATIVOS para IMAGEN (Píxeles)
        ui->spinMarginLeft->setValue(60.0);
        ui->spinMarginRight->setValue(60.0);
        ui->spinMarginTop->setValue(40.0);
        ui->spinMarginBottom->setValue(30.0);
    }
    else
    {
        // Márgenes predefinidos NATIVOS para PDF (Milímetros)
        ui->spinMarginLeft->setValue(15.0);
        ui->spinMarginRight->setValue(15.0);
        ui->spinMarginTop->setValue(10.0);
        ui->spinMarginBottom->setValue(5.0);
    }

    ui->spinMarginLeft->blockSignals(false);
    ui->spinMarginRight->blockSignals(false);
    ui->spinMarginTop->blockSignals(false);
    ui->spinMarginBottom->blockSignals(false);
}

//////////////////////////////////////////////////////////////////
// Vista previa
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::updatePreview()
{
    updateOptions();

    SkyChartExporter* exp = getExporter();
    if (!exp)
    {
        setStatus(tr("Módulo no encontrado"));
        return;
    }

   if (!previewDialog)
    {
        // Aislamiento total: Creamos la ventana sin un "parent" de Stellarium para evitar el borde verde.
        previewDialog = new PreviewDialog(nullptr);
        
        // Atributo de ventana: Forzamos a que esta ventana huérfana flote permanentemente 
        // por encima de Stellarium y otras ventanas, recuperando el comportamiento deseado.
        previewDialog->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    }

    qDebug() << "[DEBUG-2 Dialog->Preview] Enviando" << options.specificTargets.size() << "targets:";
    for (const auto& t : options.specificTargets)
        qDebug() << "   ->" << t.id << t.displayName
                  << "font=" << t.font.family() << t.font.pointSize()
                  << "bold=" << t.bold << "italic=" << t.italic
                  << "mag=" << t.limitingMagnitude;

    previewDialog->show();
    previewDialog->raise();
    previewDialog->activateWindow();
    previewDialog->updatePreviewData(exp, options, this);

    setStatus(tr("Vista previa desplegada"));
}

//////////////////////////////////////////////////////////////////
// Exportar PDF
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::exportPdf()
{
    updateOptions();

    SkyChartExporter* exp = getExporter();
    if (!exp)
    {
        setStatus(tr("No se encontró el módulo SkyChartExporter"));
        return;
    }

    if (previewDialog && previewDialog->isVisible())
        previewDialog->hide();

    // 1. Filtrar dinámicamente formatos según el modo seleccionado
    QString filter;
    if (ui->buttonFormatPDF->isChecked()) {
        filter = tr("PDF (*.pdf);;SVG (*.svg)");
    } else {
        filter = tr("JPEG (*.jpg *.jpeg);;PNG (*.png)");
    }

    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        nullptr,  
        tr("Exportar carta celeste"),
        QString(),
        filter,
        &selectedFilter);

    if (filePath.isEmpty())
        return;

    // 2. Determinar la extensión y formato correcto
    if (selectedFilter.contains("PDF")) {
        if (!filePath.endsWith(".pdf", Qt::CaseInsensitive)) filePath += ".pdf";
        options.exportFormat = "PDF";
    } else if (selectedFilter.contains("SVG")) {
        if (!filePath.endsWith(".svg", Qt::CaseInsensitive)) filePath += ".svg";
        options.exportFormat = "SVG";
    } else if (selectedFilter.contains("JPEG") || selectedFilter.contains("JPG")) {
        if (!filePath.endsWith(".jpg", Qt::CaseInsensitive) && !filePath.endsWith(".jpeg", Qt::CaseInsensitive)) filePath += ".jpg";
        options.exportFormat = "JPEG";
    } else if (selectedFilter.contains("PNG")) {
        if (!filePath.endsWith(".png", Qt::CaseInsensitive)) filePath += ".png";
        options.exportFormat = "PNG";
    }

    // 3. ¡Llamamos al despachador universal!
    bool ok = exp->exportChart(filePath, this->options);

    if (ok)
        setStatus(tr("Exportado: %1").arg(filePath));
    else
        setStatus(tr("Error al exportar"));
}


//////////////////////////////////////////////////////////////////
// Barra de estado
//////////////////////////////////////////////////////////////////

void SkyChartExporterDialog::setStatus(const QString& text)
{
    ui->labelStatus->setText(
        tr("Estado: %1").arg(text));

    qDebug() << "[SkyChartExporter]" << text;
}