#include "views/CustomizeDialog.h"
#include "ui_customize.h"
#include <QSettings>
#include <QColorDialog>
#include <QDebug>
#include <QPushButton>
#include <QListWidgetItem>

CustomizeDialog::CustomizeDialog(const QString &windowType, QWidget *targetWindow, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CustomizeDialog)
    , m_windowType(windowType)
    , m_targetWindow(targetWindow)
{
    ui->setupUi(this);
    
    // Set dialog properties
    setWindowTitle(QString("Customize - %1").arg(windowType));
    setModal(true);
    setFixedSize(380, 420);
    
    // Setup connections
    setupConnections();
    
    // Load saved settings
    loadSettings();
    
    qDebug() << "[CustomizeDialog] Created for window type:" << windowType;
}

CustomizeDialog::~CustomizeDialog()
{
    delete ui;
}

void CustomizeDialog::setupConnections()
{
    // Connect button box signals - save and apply on OK
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        applySettings();
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &CustomizeDialog::reject);
    
    // Connect Apply button - save and apply without closing
    QPushButton *applyBtn = ui->buttonBox->button(QDialogButtonBox::Apply);
    if (applyBtn) {
        connect(applyBtn, &QPushButton::clicked, this, &CustomizeDialog::onApplyClicked);
    }
    
    // Connect color picker button
    connect(ui->colorPickerButton, &QPushButton::clicked, this, &CustomizeDialog::onColorPickerClicked);
    
    // Connect reset button
    connect(ui->resetButton, &QPushButton::clicked, this, &CustomizeDialog::onResetClicked);
    
    // Update preview when attribute selection changes
    connect(ui->attributeListWidget, &QListWidget::currentRowChanged, this, [this]() {
        QListWidgetItem *item = ui->attributeListWidget->currentItem();
        if (item) {
            ui->previewTextLabel->setText(item->text());
        }
    });
}

void CustomizeDialog::loadSettings()
{
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup(QString("Customize/%1").arg(m_windowType));
    
    // Load font
    QString font = settings.value("font", "Tahoma").toString();
    int fontIndex = ui->fontComboBox->findText(font);
    if (fontIndex >= 0) {
        ui->fontComboBox->setCurrentIndex(fontIndex);
    }
    
    // Load general settings
    ui->titleBarCheckBox->setChecked(settings.value("titleBar", true).toBool());
    ui->toolBarCheckBox->setChecked(settings.value("toolBar", false).toBool());
    ui->labelsOnCheckBox->setChecked(settings.value("labelsOn", false).toBool());
    ui->gridCheckBox->setChecked(settings.value("grid", false).toBool());
    
    // Load open view with setting
    QString openWith = settings.value("openWith", "default").toString();
    if (openWith == "default") {
        ui->defaultRadio->setChecked(true);
    } else if (openWith == "sizePosition") {
        ui->sizePositionRadio->setChecked(true);
    } else if (openWith == "lastSaved") {
        ui->lastSavedRadio->setChecked(true);
    }
    
    // Load color settings
    QString bgColor = settings.value("backgroundColor", "#0000ff").toString();
    QString fgColor = settings.value("foregroundColor", "#ffffff").toString();
    
    ui->colorPickerButton->setStyleSheet(QString("background-color: %1; border: 1px solid #000000;").arg(bgColor));
    ui->previewTextLabel->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #000000;").arg(bgColor, fgColor));
    
    settings.endGroup();
    
    qDebug() << "[CustomizeDialog] Loaded settings for" << m_windowType;
}

void CustomizeDialog::saveSettings()
{
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup(QString("Customize/%1").arg(m_windowType));
    
    // Save font
    settings.setValue("font", ui->fontComboBox->currentText());
    
    // Save general settings
    settings.setValue("titleBar", ui->titleBarCheckBox->isChecked());
    settings.setValue("toolBar", ui->toolBarCheckBox->isChecked());
    settings.setValue("labelsOn", ui->labelsOnCheckBox->isChecked());
    settings.setValue("grid", ui->gridCheckBox->isChecked());
    
    // Save open view with setting
    QString openWith = "default";
    if (ui->sizePositionRadio->isChecked()) {
        openWith = "sizePosition";
    } else if (ui->lastSavedRadio->isChecked()) {
        openWith = "lastSaved";
    }
    settings.setValue("openWith", openWith);
    
    // Extract and save colors from stylesheet
    QString previewStyle = ui->previewTextLabel->styleSheet();
    QString bgColor = "#0000ff";
    QString fgColor = "#ffffff";
    
    // Parse background-color from stylesheet
    int bgStart = previewStyle.indexOf("background-color: ") + 18;
    int bgEnd = previewStyle.indexOf(";", bgStart);
    if (bgStart > 17 && bgEnd > bgStart) {
        bgColor = previewStyle.mid(bgStart, bgEnd - bgStart);
    }
    
    // Parse color (foreground) from stylesheet
    int fgStart = previewStyle.indexOf("color: ") + 7;
    int fgEnd = previewStyle.indexOf(";", fgStart);
    if (fgStart > 6 && fgEnd > fgStart) {
        fgColor = previewStyle.mid(fgStart, fgEnd - fgStart);
    }
    
    settings.setValue("backgroundColor", bgColor);
    settings.setValue("foregroundColor", fgColor);
    
    settings.endGroup();
    
    qDebug() << "[CustomizeDialog] Saved settings for" << m_windowType;
}

void CustomizeDialog::onApplyClicked()
{
    saveSettings();
    applySettings();
    qDebug() << "[CustomizeDialog] Applied settings for" << m_windowType;
}

void CustomizeDialog::onColorPickerClicked()
{
    // Determine if we're selecting background or foreground color
    QString colorFor = ui->colorForComboBox->currentText();
    
    // Get current color from preview
    QString currentStyle = ui->previewTextLabel->styleSheet();
    QColor currentColor = Qt::blue;
    
    if (colorFor == "Background") {
        // Extract background color
        int bgStart = currentStyle.indexOf("background-color: ") + 18;
        int bgEnd = currentStyle.indexOf(";", bgStart);
        if (bgStart > 17 && bgEnd > bgStart) {
            QString colorStr = currentStyle.mid(bgStart, bgEnd - bgStart).trimmed();
            currentColor = QColor(colorStr);
        }
    } else {
        // Extract foreground color
        int fgStart = currentStyle.indexOf("color: ") + 7;
        int fgEnd = currentStyle.indexOf(";", fgStart);
        if (fgStart > 6 && fgEnd > fgStart) {
            QString colorStr = currentStyle.mid(fgStart, fgEnd - fgStart).trimmed();
            currentColor = QColor(colorStr);
        }
    }
    
    // Open color dialog
    QColor newColor = QColorDialog::getColor(currentColor, this, QString("Select %1 Color").arg(colorFor));
    
    if (newColor.isValid()) {
        QString colorName = newColor.name();
        
        if (colorFor == "Background") {
            // Update background color
            ui->colorPickerButton->setStyleSheet(QString("background-color: %1; border: 1px solid #000000;").arg(colorName));
            
            // Extract current foreground color
            QString fgColor = "#ffffff";
            int fgStart = currentStyle.indexOf("color: ") + 7;
            int fgEnd = currentStyle.indexOf(";", fgStart);
            if (fgStart > 6 && fgEnd > fgStart) {
                fgColor = currentStyle.mid(fgStart, fgEnd - fgStart).trimmed();
            }
            
            ui->previewTextLabel->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #000000;").arg(colorName, fgColor));
        } else {
            // Update foreground color
            // Extract current background color
            QString bgColor = "#0000ff";
            int bgStart = currentStyle.indexOf("background-color: ") + 18;
            int bgEnd = currentStyle.indexOf(";", bgStart);
            if (bgStart > 17 && bgEnd > bgStart) {
                bgColor = currentStyle.mid(bgStart, bgEnd - bgStart).trimmed();
            }
            
            ui->previewTextLabel->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid #000000;").arg(bgColor, colorName));
        }
        
        qDebug() << "[CustomizeDialog] Selected" << colorFor << "color:" << colorName;
    }
}

void CustomizeDialog::onResetClicked()
{
    // Reset to default values
    ui->fontComboBox->setCurrentText("Tahoma");
    ui->titleBarCheckBox->setChecked(true);
    ui->toolBarCheckBox->setChecked(false);
    ui->labelsOnCheckBox->setChecked(false);
    ui->gridCheckBox->setChecked(false);
    ui->defaultRadio->setChecked(true);
    
    // Reset colors
    ui->colorPickerButton->setStyleSheet("background-color: #0000ff; border: solid 1px #000000;");
    ui->previewTextLabel->setStyleSheet("background-color: #0000ff; color: #ffffff; border: solid 1px #000000;");
    
    qDebug() << "[CustomizeDialog] Reset to defaults";
}

void CustomizeDialog::applySettings()
{
    if (!m_targetWindow) {
        qDebug() << "[CustomizeDialog] ERROR: No target window to apply settings to";
        return;
    }
    
    qDebug() << "[CustomizeDialog] Applying settings to target window:" << m_targetWindow->metaObject()->className();
    
    // Extract background color from preview
    QString previewStyle = ui->previewTextLabel->styleSheet();
    QString bgColor = "#ffffff";  // default
    
    // Parse background color from stylesheet
    int bgStart = previewStyle.indexOf("background-color: ") + 18;
    int bgEnd = previewStyle.indexOf(";", bgStart);
    if (bgStart > 17 && bgEnd > bgStart) {
        bgColor = previewStyle.mid(bgStart, bgEnd - bgStart).trimmed();
    }
    
    qDebug() << "[CustomizeDialog] Extracted background color:" << bgColor;
    qDebug() << "[CustomizeDialog] Preview stylesheet:" << previewStyle;
    
    // Find the actual widget to style
    // Many windows (BuyWindow, SellWindow) have a m_formWidget child that's the actual content
    QWidget *widgetToStyle = m_targetWindow;
    
    // Debug: List ALL children to see what's there
    qDebug() << "[CustomizeDialog] Searching for form widget among children...";
    QObjectList children = m_targetWindow->children();
    qDebug() << "[CustomizeDialog] Total children count:" << children.size();
    
    for (int i = 0; i < children.size(); ++i) {
        QObject *child = children[i];
        QWidget *childWidget = qobject_cast<QWidget*>(child);
        if (childWidget) {
            QString className = childWidget->metaObject()->className();
            QString objName = childWidget->objectName();
            qDebug() << "[CustomizeDialog] Child" << i << ":" << className << "ObjectName:" << (objName.isEmpty() ? "<empty>" : objName);
            
            // Look for QWidget child (the form loaded from .ui file)
            // It's typically a direct QWidget child with QVBoxLayout parent
            if (className == QString("QWidget") && childWidget->layout() != nullptr) {
                widgetToStyle = childWidget;
                qDebug() << "[CustomizeDialog] *** FOUND FORM WIDGET! Using child:" << className;
                break;
            }
        }
    }
    
    if (widgetToStyle == m_targetWindow) {
        qDebug() << "[CustomizeDialog] WARNING: No form widget child found, applying to parent window directly";
    }
    
    // Ensure the widget can accept stylesheets
    widgetToStyle->setAttribute(Qt::WA_StyledBackground, true);
    widgetToStyle->setAutoFillBackground(true);
    
    // Apply stylesheet ONLY to the main widget background
    // Explicitly reset child widgets to prevent inheritance and preserve their original styling
    QString widgetClassName = widgetToStyle->metaObject()->className();
    QString newStyle = QString(
        "%1 { background-color: %2; }"
        "QLineEdit { background-color: white; border: 1px solid #000000; }"
        "QComboBox { background-color: white; border: 1px solid #000000; }"
        "QPushButton { }"  // Don't change button styling at all
        "QLabel { background-color: transparent; }"
        "QTableView { }"  // Don't change table view styling (MarketWatch)
        "QHeaderView { }"  // Don't change header styling
    ).arg(widgetClassName, bgColor);
    
    widgetToStyle->setStyleSheet(newStyle);
    
    // Also set using QPalette for the window background only
    QPalette palette = widgetToStyle->palette();
    palette.setColor(QPalette::Window, QColor(bgColor));
    // Note: We do NOT set Base, Button, or AlternateBase
    // This keeps input fields with their original styling
    widgetToStyle->setPalette(palette);
    
    // Force immediate repaint
    widgetToStyle->update();
    widgetToStyle->repaint();
    
    qDebug() << "[CustomizeDialog] Applied background color" << bgColor << "to widget:" << widgetToStyle->metaObject()->className();
    qDebug() << "[CustomizeDialog] Applied stylesheet:" << newStyle;
}
