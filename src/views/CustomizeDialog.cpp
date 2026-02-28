#include "views/CustomizeDialog.h"
#include "ui_customize.h"
#include "utils/WindowSettingsHelper.h"
#include <QSettings>
#include <QColorDialog>
#include <QDebug>
#include <QPushButton>
#include <QListWidgetItem>
#include <QFont>
#include <QFontComboBox>
#include <QSpinBox>

// ── Static data ────────────────────────────────────────────────────────────
const QStringList CustomizeDialog::s_allWindowTypes = {
    "MarketWatch", "ATMWatch", "OptionChain", "OrderBook",
    "TradeBook",   "PositionWindow", "ChartWindow",
    "SnapQuote",   "OptionCalculator", "StrategyManager",
    "MarketMovement", "IndicatorChart"
};

const QStringList CustomizeDialog::s_colorAttributes = {
    "General Appearance",
    "Table Header",
    "Table Row (Even)",
    "Table Row (Odd)",
    "Selection",
    "Grid Lines",
    "Labels",
    "Positive / Up",
    "Negative / Down",
    "Title Bar"
};

// ── Constructor / Destructor ───────────────────────────────────────────────
CustomizeDialog::CustomizeDialog(const QString &windowType,
                                 QWidget *targetWindow,
                                 QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CustomizeDialog)
    , m_windowType(windowType)
    , m_targetWindow(targetWindow)
{
    ui->setupUi(this);

    setWindowTitle(QString("Customize — %1").arg(windowType));
    setModal(true);

    // Populate dynamic widgets
    populateViewComboBox();
    populateAttributeList();

    // Wire signals
    setupConnections();

    // Load persisted settings into the dialog
    loadSettings();

    // Ensure previews are up-to-date
    updateColorPreview();
    updateFontPreview();

    qDebug() << "[CustomizeDialog] Created for window type:" << windowType;
}

CustomizeDialog::~CustomizeDialog()
{
    delete ui;
}

// ── Populate helpers ───────────────────────────────────────────────────────
void CustomizeDialog::populateViewComboBox()
{
    // "All Views" is already in the .ui as the first item.
    // Add each known window type after it.
    for (const QString &wt : s_allWindowTypes) {
        ui->viewComboBox->addItem(wt);
    }
    // Select the current window type
    int idx = ui->viewComboBox->findText(m_windowType);
    if (idx >= 0)
        ui->viewComboBox->setCurrentIndex(idx);
}

void CustomizeDialog::populateAttributeList()
{
    ui->attributeListWidget->clear();
    for (const QString &attr : s_colorAttributes) {
        ui->attributeListWidget->addItem(attr);
    }
    if (ui->attributeListWidget->count() > 0)
        ui->attributeListWidget->setCurrentRow(0);
}

// ── Connections ────────────────────────────────────────────────────────────
void CustomizeDialog::setupConnections()
{
    // OK / Cancel
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        applySettings();
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &CustomizeDialog::reject);

    // Apply button
    QPushButton *applyBtn = ui->buttonBox->button(QDialogButtonBox::Apply);
    if (applyBtn) {
        connect(applyBtn, &QPushButton::clicked, this, &CustomizeDialog::onApplyClicked);
    }

    // Color tab
    connect(ui->colorPickerButton, &QPushButton::clicked,
            this, &CustomizeDialog::onColorPickerClicked);
    connect(ui->attributeListWidget, &QListWidget::currentRowChanged,
            this, &CustomizeDialog::onAttributeSelected);
    connect(ui->colorForComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CustomizeDialog::onColorForChanged);
    connect(ui->viewComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CustomizeDialog::onViewChanged);

    // Font tab
    connect(ui->fontComboBox, &QFontComboBox::currentFontChanged,
            this, [this](const QFont &) { onFontChanged(); });
    connect(ui->fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CustomizeDialog::onFontSizeChanged);
    connect(ui->boldCheckBox, &QCheckBox::toggled,
            this, [this](bool) { onFontStyleChanged(); });
    connect(ui->italicCheckBox, &QCheckBox::toggled,
            this, [this](bool) { onFontStyleChanged(); });

    // Reset
    connect(ui->resetButton, &QPushButton::clicked, this, &CustomizeDialog::onResetClicked);
}

// ── Load / Save ────────────────────────────────────────────────────────────
void CustomizeDialog::loadSettings()
{
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup(QString("Customize/%1").arg(m_windowType));

    // ── Font tab ──
    QString fontFamily = settings.value("font", "Tahoma").toString();
    int fontIdx = ui->fontComboBox->findText(fontFamily);
    if (fontIdx >= 0)
        ui->fontComboBox->setCurrentIndex(fontIdx);

    ui->fontSizeSpinBox->setValue(settings.value("fontSize", 12).toInt());
    ui->boldCheckBox->setChecked(settings.value("fontBold", false).toBool());
    ui->italicCheckBox->setChecked(settings.value("fontItalic", false).toBool());

    // ── Layout tab ──
    ui->titleBarCheckBox->setChecked(settings.value("titleBar", true).toBool());
    ui->toolBarCheckBox->setChecked(settings.value("toolBar", false).toBool());
    ui->labelsOnCheckBox->setChecked(settings.value("labelsOn", false).toBool());
    ui->gridCheckBox->setChecked(settings.value("grid", false).toBool());

    QString openWith = settings.value("openWith", "default").toString();
    if (openWith == "sizePosition")
        ui->sizePositionRadio->setChecked(true);
    else if (openWith == "lastSaved")
        ui->lastSavedRadio->setChecked(true);
    else
        ui->defaultRadio->setChecked(true);

    // ── Color tab — load per-attribute colors ──
    m_colorMap.clear();
    for (const QString &attr : s_colorAttributes) {
        QString bgKey = QString("color/%1/Background").arg(attr);
        QString fgKey = QString("color/%1/Foreground").arg(attr);

        QColor bg = QColor(settings.value(bgKey, "#ffffff").toString());
        QColor fg = QColor(settings.value(fgKey, "#1e293b").toString());

        m_colorMap[QString("%1/Background").arg(attr)] = bg;
        m_colorMap[QString("%1/Foreground").arg(attr)] = fg;
    }

    // Legacy compat: if old-style backgroundColor / foregroundColor exist,
    // import them into the "General Appearance" attribute
    if (settings.contains("backgroundColor") && !settings.contains("color/General Appearance/Background")) {
        QColor bg(settings.value("backgroundColor").toString());
        QColor fg(settings.value("foregroundColor", "#ffffff").toString());
        m_colorMap["General Appearance/Background"] = bg;
        m_colorMap["General Appearance/Foreground"] = fg;
    }

    settings.endGroup();

    qDebug() << "[CustomizeDialog] Loaded settings for" << m_windowType;
}

void CustomizeDialog::saveSettings()
{
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup(QString("Customize/%1").arg(m_windowType));

    // ── Font ──
    settings.setValue("font", ui->fontComboBox->currentText());
    settings.setValue("fontSize", ui->fontSizeSpinBox->value());
    settings.setValue("fontBold", ui->boldCheckBox->isChecked());
    settings.setValue("fontItalic", ui->italicCheckBox->isChecked());

    // ── Layout ──
    settings.setValue("titleBar", ui->titleBarCheckBox->isChecked());
    settings.setValue("toolBar", ui->toolBarCheckBox->isChecked());
    settings.setValue("labelsOn", ui->labelsOnCheckBox->isChecked());
    settings.setValue("grid", ui->gridCheckBox->isChecked());

    QString openWith = "default";
    if (ui->sizePositionRadio->isChecked())
        openWith = "sizePosition";
    else if (ui->lastSavedRadio->isChecked())
        openWith = "lastSaved";
    settings.setValue("openWith", openWith);

    // ── Colors ──
    for (auto it = m_colorMap.constBegin(); it != m_colorMap.constEnd(); ++it) {
        // it.key() is e.g. "General Appearance/Background"
        settings.setValue(QString("color/%1").arg(it.key()), it.value().name());
    }

    // Also write the legacy keys so WindowSettingsHelper::applyCustomization
    // continues to work for "General Appearance"
    QColor genBg = m_colorMap.value("General Appearance/Background", QColor("#ffffff"));
    QColor genFg = m_colorMap.value("General Appearance/Foreground", QColor("#1e293b"));
    settings.setValue("backgroundColor", genBg.name());
    settings.setValue("foregroundColor", genFg.name());

    settings.endGroup();

    // ── "Apply to all Views" ──
    if (ui->applyToAllViewsCheckBox->isChecked()) {
        for (const QString &wt : s_allWindowTypes) {
            if (wt == m_windowType) continue;  // already saved above

            QSettings s2("TradingCompany", "TradingTerminal");
            s2.beginGroup(QString("Customize/%1").arg(wt));

            s2.setValue("font", ui->fontComboBox->currentText());
            s2.setValue("fontSize", ui->fontSizeSpinBox->value());
            s2.setValue("fontBold", ui->boldCheckBox->isChecked());
            s2.setValue("fontItalic", ui->italicCheckBox->isChecked());

            for (auto it = m_colorMap.constBegin(); it != m_colorMap.constEnd(); ++it) {
                s2.setValue(QString("color/%1").arg(it.key()), it.value().name());
            }

            QColor bg2 = m_colorMap.value("General Appearance/Background", QColor("#ffffff"));
            QColor fg2 = m_colorMap.value("General Appearance/Foreground", QColor("#1e293b"));
            s2.setValue("backgroundColor", bg2.name());
            s2.setValue("foregroundColor", fg2.name());

            s2.endGroup();
        }
        qDebug() << "[CustomizeDialog] Applied settings to ALL views";
    }

    qDebug() << "[CustomizeDialog] Saved settings for" << m_windowType;
}

// ── Apply to target window ─────────────────────────────────────────────────
void CustomizeDialog::applySettings()
{
    if (!m_targetWindow) {
        qDebug() << "[CustomizeDialog] ERROR: No target window to apply settings to";
        return;
    }

    // Delegate to the existing helper for consistent application
    WindowSettingsHelper::applyCustomization(m_targetWindow, m_windowType);

    qDebug() << "[CustomizeDialog] Applied settings to" << m_windowType;
}

void CustomizeDialog::onApplyClicked()
{
    saveSettings();
    applySettings();
    qDebug() << "[CustomizeDialog] Apply clicked for" << m_windowType;
}

// ── Color tab slots ────────────────────────────────────────────────────────
void CustomizeDialog::onAttributeSelected(int row)
{
    Q_UNUSED(row);
    updateColorSwatchButton();
    updateColorPreview();
}

void CustomizeDialog::onColorForChanged(int index)
{
    Q_UNUSED(index);
    updateColorSwatchButton();
}

void CustomizeDialog::onViewChanged(int index)
{
    Q_UNUSED(index);
    // If a specific view is selected (not "All Views"), reload settings for it
    QString view = ui->viewComboBox->currentText();
    if (view != "All Views" && view != m_windowType) {
        // Temporarily switch context to load that view's colors
        QString oldType = m_windowType;
        m_windowType = view;
        loadSettings();
        updateColorPreview();
        updateFontPreview();
        m_windowType = oldType;
    } else if (view == m_windowType || view == "All Views") {
        loadSettings();
        updateColorPreview();
        updateFontPreview();
    }
}

void CustomizeDialog::onColorPickerClicked()
{
    QString attrKey = currentAttributeKey();
    if (attrKey.isEmpty()) return;

    QString colorType = ui->colorForComboBox->currentText();  // "Background" or "Foreground"
    QColor current = colorForAttribute(attrKey, colorType);

    QColor chosen = QColorDialog::getColor(current, this,
        QString("Select %1 Color — %2").arg(colorType, attrKey));

    if (chosen.isValid()) {
        setColorForAttribute(attrKey, colorType, chosen);
        updateColorSwatchButton();
        updateColorPreview();
        qDebug() << "[CustomizeDialog] Set" << attrKey << colorType << "to" << chosen.name();
    }
}

QString CustomizeDialog::currentAttributeKey() const
{
    QListWidgetItem *item = ui->attributeListWidget->currentItem();
    return item ? item->text() : QString();
}

QColor CustomizeDialog::colorForAttribute(const QString &attrKey, const QString &colorType) const
{
    QString key = QString("%1/%2").arg(attrKey, colorType);
    if (m_colorMap.contains(key))
        return m_colorMap.value(key);
    return (colorType == "Background") ? QColor("#ffffff") : QColor("#1e293b");
}

void CustomizeDialog::setColorForAttribute(const QString &attrKey, const QString &colorType, const QColor &color)
{
    m_colorMap[QString("%1/%2").arg(attrKey, colorType)] = color;
}

void CustomizeDialog::updateColorSwatchButton()
{
    QString attrKey = currentAttributeKey();
    QString colorType = ui->colorForComboBox->currentText();
    QColor c = colorForAttribute(attrKey, colorType);
    ui->colorPickerButton->setStyleSheet(
        QString("background-color: %1; border: 2px solid #e2e8f0; border-radius: 4px;").arg(c.name()));
}

void CustomizeDialog::updateColorPreview()
{
    QString attrKey = currentAttributeKey();
    if (attrKey.isEmpty()) attrKey = "General Appearance";

    QColor bg = colorForAttribute(attrKey, "Background");
    QColor fg = colorForAttribute(attrKey, "Foreground");

    ui->previewTextLabel->setStyleSheet(
        QString("background-color: %1; color: %2; border: 1px solid #e2e8f0; "
                "border-radius: 4px; padding: 8px; font-size: %3pt;")
            .arg(bg.name(), fg.name())
            .arg(ui->fontSizeSpinBox->value()));

    // Also update font on the preview
    QFont f(ui->fontComboBox->currentText(), ui->fontSizeSpinBox->value());
    f.setBold(ui->boldCheckBox->isChecked());
    f.setItalic(ui->italicCheckBox->isChecked());
    ui->previewTextLabel->setFont(f);

    ui->previewTextLabel->setText(QString("Preview — %1").arg(attrKey));
}

// ── Font tab slots ─────────────────────────────────────────────────────────
void CustomizeDialog::onFontChanged()
{
    updateFontPreview();
    updateColorPreview();  // color preview also uses font
}

void CustomizeDialog::onFontSizeChanged(int size)
{
    Q_UNUSED(size);
    updateFontPreview();
    updateColorPreview();
}

void CustomizeDialog::onFontStyleChanged()
{
    updateFontPreview();
    updateColorPreview();
}

void CustomizeDialog::updateFontPreview()
{
    QFont f(ui->fontComboBox->currentText(), ui->fontSizeSpinBox->value());
    f.setBold(ui->boldCheckBox->isChecked());
    f.setItalic(ui->italicCheckBox->isChecked());

    ui->fontPreviewLabel->setFont(f);
    ui->fontPreviewLabel->setText(
        QString("%1  %2pt%3%4\nAaBbCcDd 0123456789\nThe quick brown fox jumps over the lazy dog")
            .arg(f.family())
            .arg(f.pointSize())
            .arg(f.bold() ? " Bold" : "")
            .arg(f.italic() ? " Italic" : ""));
}

// ── Reset ──────────────────────────────────────────────────────────────────
void CustomizeDialog::onResetClicked()
{
    // Font defaults
    ui->fontComboBox->setCurrentText("Tahoma");
    ui->fontSizeSpinBox->setValue(12);
    ui->boldCheckBox->setChecked(false);
    ui->italicCheckBox->setChecked(false);

    // Layout defaults
    ui->titleBarCheckBox->setChecked(true);
    ui->toolBarCheckBox->setChecked(false);
    ui->labelsOnCheckBox->setChecked(false);
    ui->gridCheckBox->setChecked(false);
    ui->defaultRadio->setChecked(true);

    // Color defaults — reset all attributes to white bg / dark fg
    m_colorMap.clear();
    for (const QString &attr : s_colorAttributes) {
        m_colorMap[QString("%1/Background").arg(attr)] = QColor("#ffffff");
        m_colorMap[QString("%1/Foreground").arg(attr)] = QColor("#1e293b");
    }
    // Special defaults for specific attributes
    m_colorMap["Selection/Background"]     = QColor("#dbeafe");
    m_colorMap["Selection/Foreground"]      = QColor("#1e40af");
    m_colorMap["Table Header/Background"]   = QColor("#f8fafc");
    m_colorMap["Table Header/Foreground"]   = QColor("#1e293b");
    m_colorMap["Table Row (Odd)/Background"]  = QColor("#f8fafc");
    m_colorMap["Grid Lines/Foreground"]     = QColor("#e2e8f0");
    m_colorMap["Positive / Up/Foreground"]  = QColor("#16a34a");
    m_colorMap["Negative / Down/Foreground"] = QColor("#dc2626");
    m_colorMap["Title Bar/Background"]      = QColor("#1e40af");
    m_colorMap["Title Bar/Foreground"]      = QColor("#ffffff");

    updateColorSwatchButton();
    updateColorPreview();
    updateFontPreview();

    qDebug() << "[CustomizeDialog] Reset to defaults";
}
