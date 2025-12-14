# UI Customization Implementation Guide

## Overview
This guide explains how to implement user-configurable UI settings for MarketWatchWindow, including grid visibility, alternate row colors, fonts, and color schemes.

## Features to Implement

### 1. Visual Settings
- ✅ Show/Hide grid lines
- ✅ Alternate row colors (zebra striping)
- ✅ Font selection (family, size, weight)
- ✅ Background color
- ✅ Text color
- ✅ Positive change color (green)
- ✅ Negative change color (red)
- ✅ Selection highlight color

### 2. Persistence
- ✅ Save settings per Market Watch window
- ✅ Global default settings
- ✅ QSettings for cross-session persistence

---

## Implementation Steps

### Step 1: Create MarketWatchSettings Class

Create `include/ui/MarketWatchSettings.h`:

```cpp
#ifndef MARKETWATCHSETTINGS_H
#define MARKETWATCHSETTINGS_H

#include <QString>
#include <QFont>
#include <QColor>
#include <QSettings>

/**
 * @brief Settings for Market Watch visual customization
 */
class MarketWatchSettings
{
public:
    MarketWatchSettings();
    
    // Visual settings
    bool showGrid;
    bool alternateRowColors;
    QFont font;
    QColor backgroundColor;
    QColor textColor;
    QColor positiveColor;
    QColor negativeColor;
    QColor selectionColor;
    QColor gridColor;
    
    // Row height
    int rowHeight;  // -1 for auto
    
    // Default values
    static MarketWatchSettings defaultSettings();
    
    // Persistence
    void save(const QString &watchName = "default") const;
    static MarketWatchSettings load(const QString &watchName = "default");
    
    // Conversion
    QVariantMap toVariantMap() const;
    void fromVariantMap(const QVariantMap &map);
    
private:
    void initializeDefaults();
};

#endif // MARKETWATCHSETTINGS_H
```

Create `src/ui/MarketWatchSettings.cpp`:

```cpp
#include "ui/MarketWatchSettings.h"

MarketWatchSettings::MarketWatchSettings()
{
    initializeDefaults();
}

void MarketWatchSettings::initializeDefaults()
{
    // Dark theme defaults
    showGrid = true;
    alternateRowColors = true;
    
    font = QFont("Consolas", 10);
    font.setStyleHint(QFont::Monospace);
    
    backgroundColor = QColor("#1e1e1e");
    textColor = QColor("#ffffff");
    positiveColor = QColor("#4ec9b0");  // Teal green
    negativeColor = QColor("#f48771");  // Coral red
    selectionColor = QColor("#094771");  // Blue
    gridColor = QColor("#3f3f46");  // Dark gray
    
    rowHeight = -1;  // Auto
}

MarketWatchSettings MarketWatchSettings::defaultSettings()
{
    return MarketWatchSettings();
}

void MarketWatchSettings::save(const QString &watchName) const
{
    QSettings settings("TradingTerminal", "MarketWatch");
    settings.beginGroup(watchName);
    
    QVariantMap map = toVariantMap();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    
    settings.endGroup();
    settings.sync();
}

MarketWatchSettings MarketWatchSettings::load(const QString &watchName)
{
    QSettings settings("TradingTerminal", "MarketWatch");
    settings.beginGroup(watchName);
    
    MarketWatchSettings s;
    
    if (settings.contains("showGrid")) {
        // Settings exist, load them
        s.showGrid = settings.value("showGrid", true).toBool();
        s.alternateRowColors = settings.value("alternateRowColors", true).toBool();
        s.font.fromString(settings.value("font", s.font.toString()).toString());
        s.backgroundColor = settings.value("backgroundColor", s.backgroundColor).value<QColor>();
        s.textColor = settings.value("textColor", s.textColor).value<QColor>();
        s.positiveColor = settings.value("positiveColor", s.positiveColor).value<QColor>();
        s.negativeColor = settings.value("negativeColor", s.negativeColor).value<QColor>();
        s.selectionColor = settings.value("selectionColor", s.selectionColor).value<QColor>();
        s.gridColor = settings.value("gridColor", s.gridColor).value<QColor>();
        s.rowHeight = settings.value("rowHeight", -1).toInt();
    }
    
    settings.endGroup();
    return s;
}

QVariantMap MarketWatchSettings::toVariantMap() const
{
    QVariantMap map;
    map["showGrid"] = showGrid;
    map["alternateRowColors"] = alternateRowColors;
    map["font"] = font.toString();
    map["backgroundColor"] = backgroundColor;
    map["textColor"] = textColor;
    map["positiveColor"] = positiveColor;
    map["negativeColor"] = negativeColor;
    map["selectionColor"] = selectionColor;
    map["gridColor"] = gridColor;
    map["rowHeight"] = rowHeight;
    return map;
}

void MarketWatchSettings::fromVariantMap(const QVariantMap &map)
{
    showGrid = map.value("showGrid", true).toBool();
    alternateRowColors = map.value("alternateRowColors", true).toBool();
    font.fromString(map.value("font", font.toString()).toString());
    backgroundColor = map.value("backgroundColor", backgroundColor).value<QColor>();
    textColor = map.value("textColor", textColor).value<QColor>();
    positiveColor = map.value("positiveColor", positiveColor).value<QColor>();
    negativeColor = map.value("negativeColor", negativeColor).value<QColor>();
    selectionColor = map.value("selectionColor", selectionColor).value<QColor>();
    gridColor = map.value("gridColor", gridColor).value<QColor>();
    rowHeight = map.value("rowHeight", -1).toInt();
}
```

---

### Step 2: Create Settings Dialog

Create `include/ui/MarketWatchSettingsDialog.h`:

```cpp
#ifndef MARKETWATCHSETTINGSDIALOG_H
#define MARKETWATCHSETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QFontComboBox>
#include <QSpinBox>
#include "ui/MarketWatchSettings.h"

class MarketWatchSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MarketWatchSettingsDialog(const MarketWatchSettings &settings, 
                                       QWidget *parent = nullptr);
    
    MarketWatchSettings getSettings() const;

private slots:
    void onFontSelect();
    void onColorSelect(const QString &colorType);
    void onRestoreDefaults();

private:
    void setupUI();
    void applySettings(const MarketWatchSettings &settings);
    void updateColorButton(QPushButton *button, const QColor &color);
    
    MarketWatchSettings m_settings;
    
    // UI Controls
    QCheckBox *m_showGridCheck;
    QCheckBox *m_alternateRowsCheck;
    QPushButton *m_fontButton;
    QPushButton *m_backgroundColorButton;
    QPushButton *m_textColorButton;
    QPushButton *m_positiveColorButton;
    QPushButton *m_negativeColorButton;
    QPushButton *m_selectionColorButton;
    QPushButton *m_gridColorButton;
    QSpinBox *m_rowHeightSpin;
};

#endif // MARKETWATCHSETTINGSDIALOG_H
```

Create `src/ui/MarketWatchSettingsDialog.cpp`:

```cpp
#include "ui/MarketWatchSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFontDialog>
#include <QColorDialog>
#include <QDialogButtonBox>

MarketWatchSettingsDialog::MarketWatchSettingsDialog(const MarketWatchSettings &settings,
                                                     QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setupUI();
    applySettings(settings);
}

void MarketWatchSettingsDialog::setupUI()
{
    setWindowTitle("Market Watch Settings");
    setMinimumWidth(500);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Grid Settings Group
    QGroupBox *gridGroup = new QGroupBox("Display Options", this);
    QVBoxLayout *gridLayout = new QVBoxLayout(gridGroup);
    
    m_showGridCheck = new QCheckBox("Show Grid Lines", this);
    m_alternateRowsCheck = new QCheckBox("Alternate Row Colors", this);
    
    gridLayout->addWidget(m_showGridCheck);
    gridLayout->addWidget(m_alternateRowsCheck);
    
    mainLayout->addWidget(gridGroup);
    
    // Font Settings Group
    QGroupBox *fontGroup = new QGroupBox("Font", this);
    QHBoxLayout *fontLayout = new QHBoxLayout(fontGroup);
    
    m_fontButton = new QPushButton("Select Font...", this);
    connect(m_fontButton, &QPushButton::clicked, this, &MarketWatchSettingsDialog::onFontSelect);
    fontLayout->addWidget(m_fontButton);
    fontLayout->addStretch();
    
    mainLayout->addWidget(fontGroup);
    
    // Color Settings Group
    QGroupBox *colorGroup = new QGroupBox("Colors", this);
    QVBoxLayout *colorLayout = new QVBoxLayout(colorGroup);
    
    auto createColorRow = [this, colorLayout](const QString &label, const QString &colorType) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lbl = new QLabel(label + ":", this);
        lbl->setMinimumWidth(150);
        row->addWidget(lbl);
        
        QPushButton *btn = new QPushButton("Choose Color", this);
        btn->setMinimumWidth(120);
        btn->setProperty("colorType", colorType);
        connect(btn, &QPushButton::clicked, this, [this, colorType]() {
            onColorSelect(colorType);
        });
        row->addWidget(btn);
        row->addStretch();
        
        colorLayout->addLayout(row);
        
        if (colorType == "background") m_backgroundColorButton = btn;
        else if (colorType == "text") m_textColorButton = btn;
        else if (colorType == "positive") m_positiveColorButton = btn;
        else if (colorType == "negative") m_negativeColorButton = btn;
        else if (colorType == "selection") m_selectionColorButton = btn;
        else if (colorType == "grid") m_gridColorButton = btn;
    };
    
    createColorRow("Background Color", "background");
    createColorRow("Text Color", "text");
    createColorRow("Positive Change Color", "positive");
    createColorRow("Negative Change Color", "negative");
    createColorRow("Selection Color", "selection");
    createColorRow("Grid Color", "grid");
    
    mainLayout->addWidget(colorGroup);
    
    // Row Height
    QGroupBox *layoutGroup = new QGroupBox("Layout", this);
    QHBoxLayout *layoutLayout = new QHBoxLayout(layoutGroup);
    
    QLabel *rowHeightLabel = new QLabel("Row Height:", this);
    m_rowHeightSpin = new QSpinBox(this);
    m_rowHeightSpin->setMinimum(-1);
    m_rowHeightSpin->setMaximum(100);
    m_rowHeightSpin->setSpecialValueText("Auto");
    m_rowHeightSpin->setSuffix(" px");
    
    layoutLayout->addWidget(rowHeightLabel);
    layoutLayout->addWidget(m_rowHeightSpin);
    layoutLayout->addStretch();
    
    mainLayout->addWidget(layoutGroup);
    
    // Buttons
    mainLayout->addStretch();
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *restoreButton = new QPushButton("Restore Defaults", this);
    connect(restoreButton, &QPushButton::clicked, this, &MarketWatchSettingsDialog::onRestoreDefaults);
    buttonLayout->addWidget(restoreButton);
    
    buttonLayout->addStretch();
    
    QDialogButtonBox *dialogButtons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(dialogButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonLayout->addWidget(dialogButtons);
    
    mainLayout->addLayout(buttonLayout);
    
    // Apply dark theme
    setStyleSheet(
        "QDialog { background-color: #2d2d30; color: #ffffff; }"
        "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 8px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QPushButton { background-color: #3e3e42; border: 1px solid #555555; padding: 6px 12px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #505050; }"
        "QCheckBox { spacing: 8px; }"
        "QSpinBox { background-color: #3e3e42; border: 1px solid #555555; padding: 4px; }"
    );
}

void MarketWatchSettingsDialog::applySettings(const MarketWatchSettings &settings)
{
    m_showGridCheck->setChecked(settings.showGrid);
    m_alternateRowsCheck->setChecked(settings.alternateRowColors);
    
    m_fontButton->setText(QString("%1, %2pt").arg(settings.font.family()).arg(settings.font.pointSize()));
    
    updateColorButton(m_backgroundColorButton, settings.backgroundColor);
    updateColorButton(m_textColorButton, settings.textColor);
    updateColorButton(m_positiveColorButton, settings.positiveColor);
    updateColorButton(m_negativeColorButton, settings.negativeColor);
    updateColorButton(m_selectionColorButton, settings.selectionColor);
    updateColorButton(m_gridColorButton, settings.gridColor);
    
    m_rowHeightSpin->setValue(settings.rowHeight);
}

void MarketWatchSettingsDialog::updateColorButton(QPushButton *button, const QColor &color)
{
    button->setStyleSheet(QString(
        "QPushButton { "
        "  background-color: %1; "
        "  color: %2; "
        "  border: 2px solid #555555; "
        "  padding: 6px 12px; "
        "  border-radius: 3px; "
        "}"
    ).arg(color.name()).arg(color.lightness() > 128 ? "#000000" : "#ffffff"));
    
    button->setText(color.name().toUpper());
}

void MarketWatchSettingsDialog::onFontSelect()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_settings.font, this, "Select Font");
    
    if (ok) {
        m_settings.font = font;
        m_fontButton->setText(QString("%1, %2pt").arg(font.family()).arg(font.pointSize()));
    }
}

void MarketWatchSettingsDialog::onColorSelect(const QString &colorType)
{
    QColor currentColor;
    
    if (colorType == "background") currentColor = m_settings.backgroundColor;
    else if (colorType == "text") currentColor = m_settings.textColor;
    else if (colorType == "positive") currentColor = m_settings.positiveColor;
    else if (colorType == "negative") currentColor = m_settings.negativeColor;
    else if (colorType == "selection") currentColor = m_settings.selectionColor;
    else if (colorType == "grid") currentColor = m_settings.gridColor;
    
    QColor color = QColorDialog::getColor(currentColor, this, "Select Color");
    
    if (color.isValid()) {
        if (colorType == "background") {
            m_settings.backgroundColor = color;
            updateColorButton(m_backgroundColorButton, color);
        } else if (colorType == "text") {
            m_settings.textColor = color;
            updateColorButton(m_textColorButton, color);
        } else if (colorType == "positive") {
            m_settings.positiveColor = color;
            updateColorButton(m_positiveColorButton, color);
        } else if (colorType == "negative") {
            m_settings.negativeColor = color;
            updateColorButton(m_negativeColorButton, color);
        } else if (colorType == "selection") {
            m_settings.selectionColor = color;
            updateColorButton(m_selectionColorButton, color);
        } else if (colorType == "grid") {
            m_settings.gridColor = color;
            updateColorButton(m_gridColorButton, color);
        }
    }
}

void MarketWatchSettingsDialog::onRestoreDefaults()
{
    m_settings = MarketWatchSettings::defaultSettings();
    applySettings(m_settings);
}

MarketWatchSettings MarketWatchSettingsDialog::getSettings() const
{
    MarketWatchSettings settings = m_settings;
    settings.showGrid = m_showGridCheck->isChecked();
    settings.alternateRowColors = m_alternateRowsCheck->isChecked();
    settings.rowHeight = m_rowHeightSpin->value();
    return settings;
}
```

---

### Step 3: Integrate with MarketWatchWindow

Update `include/ui/MarketWatchWindow.h`:

```cpp
// Add includes
#include "ui/MarketWatchSettings.h"

// Add public slot
public slots:
    void showSettingsDialog();
    void applySettings(const MarketWatchSettings &settings);
    
// Add private member
private:
    MarketWatchSettings m_settings;
```

Update `src/ui/MarketWatchWindow.cpp`:

```cpp
// Add to constructor
MarketWatchWindow::MarketWatchWindow(QWidget *parent)
    : QWidget(parent)
    // ... existing initialization
{
    // Load saved settings
    m_settings = MarketWatchSettings::load("default");
    
    setupUI();
    setupConnections();
    setupKeyboardShortcuts();
    
    // Apply loaded settings
    applySettings(m_settings);
}

// Implement applySettings
void MarketWatchWindow::applySettings(const MarketWatchSettings &settings)
{
    m_settings = settings;
    
    // Apply visual settings
    m_tableView->setShowGrid(settings.showGrid);
    m_tableView->setAlternatingRowColors(settings.alternateRowColors);
    m_tableView->setFont(settings.font);
    
    // Update stylesheet
    QString styleSheet = QString(
        "QTableView {"
        "    background-color: %1;"
        "    color: %2;"
        "    gridline-color: %3;"
        "    selection-background-color: %4;"
        "    selection-color: %2;"
        "}"
        "QHeaderView::section {"
        "    background-color: #2d2d30;"
        "    color: %2;"
        "    padding: 4px;"
        "    border: 1px solid %3;"
        "    font-weight: bold;"
        "}"
        "QHeaderView::section:hover {"
        "    background-color: #3e3e42;"
        "}"
    ).arg(settings.backgroundColor.name())
     .arg(settings.textColor.name())
     .arg(settings.gridColor.name())
     .arg(settings.selectionColor.name());
    
    setStyleSheet(styleSheet);
    
    // Set row height
    if (settings.rowHeight > 0) {
        m_tableView->verticalHeader()->setDefaultSectionSize(settings.rowHeight);
    } else {
        m_tableView->verticalHeader()->setDefaultSectionSize(24);  // Default
    }
    
    // Save settings
    m_settings.save("default");
    
    qDebug() << "[MarketWatchWindow] Applied settings";
}

// Implement showSettingsDialog
void MarketWatchWindow::showSettingsDialog()
{
    MarketWatchSettingsDialog dialog(m_settings, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        MarketWatchSettings newSettings = dialog.getSettings();
        applySettings(newSettings);
    }
}
```

---

### Step 4: Add Settings Menu Item

Update context menu in `showContextMenu()`:

```cpp
// Add at the end of context menu
menu.addSeparator();
menu.addAction("Settings...", this, &MarketWatchWindow::showSettingsDialog);
```

---

### Step 5: Update CMakeLists.txt

Add new files:

```cmake
set(HEADERS
    # ... existing headers ...
    include/ui/MarketWatchSettings.h
    include/ui/MarketWatchSettingsDialog.h
)

set(SOURCES
    # ... existing sources ...
    src/ui/MarketWatchSettings.cpp
    src/ui/MarketWatchSettingsDialog.cpp
)
```

---

## Testing Checklist

### Basic Functionality
- [ ] Open Settings dialog from context menu
- [ ] Toggle "Show Grid" checkbox
- [ ] Toggle "Alternate Row Colors" checkbox
- [ ] Select different font
- [ ] Change background color
- [ ] Change text color
- [ ] Change positive/negative colors
- [ ] Change selection color
- [ ] Change grid color
- [ ] Adjust row height

### Persistence
- [ ] Apply settings
- [ ] Close Market Watch window
- [ ] Create new Market Watch
- [ ] Verify settings are loaded
- [ ] Restart application
- [ ] Verify settings persist

### Visual Verification
- [ ] Grid lines show/hide correctly
- [ ] Alternate row colors work
- [ ] Font changes apply
- [ ] Colors apply immediately
- [ ] Row height adjusts correctly

### Restore Defaults
- [ ] Change multiple settings
- [ ] Click "Restore Defaults"
- [ ] Verify all settings reset to defaults

---

## Advanced Features (Optional)

### 1. Preset Themes

```cpp
// Add to MarketWatchSettings
static MarketWatchSettings darkTheme();
static MarketWatchSettings lightTheme();
static MarketWatchSettings blueTheme();

// In dialog, add theme dropdown
QComboBox *themeCombo = new QComboBox(this);
themeCombo->addItems({"Custom", "Dark", "Light", "Blue"});
connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
    if (index == 1) applySettings(MarketWatchSettings::darkTheme());
    else if (index == 2) applySettings(MarketWatchSettings::lightTheme());
    else if (index == 3) applySettings(MarketWatchSettings::blueTheme());
});
```

### 2. Per-Window Settings

```cpp
// Save settings with unique window name
m_settings.save(QString("watch_%1").arg(windowId));

// Load settings for specific window
m_settings = MarketWatchSettings::load(QString("watch_%1").arg(windowId));
```

### 3. Export/Import Settings

```cpp
// Export to JSON file
void exportSettings(const QString &filePath);

// Import from JSON file
void importSettings(const QString &filePath);
```

---

## Summary

**Files to Create**:
1. `include/ui/MarketWatchSettings.h` (150 lines)
2. `src/ui/MarketWatchSettings.cpp` (200 lines)
3. `include/ui/MarketWatchSettingsDialog.h` (60 lines)
4. `src/ui/MarketWatchSettingsDialog.cpp` (300 lines)

**Files to Modify**:
1. `include/ui/MarketWatchWindow.h` (add 2 methods, 1 member)
2. `src/ui/MarketWatchWindow.cpp` (add 2 implementations, update constructor)
3. `CMakeLists.txt` (add 4 files)

**Total New Code**: ~850 lines

**Estimated Time**: 4-6 hours including testing

**Complexity**: Medium (mostly UI code with QSettings)

**User Experience**: Professional settings dialog with live preview and persistence.
