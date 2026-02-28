#ifndef WINDOWSETTINGSHELPER_H
#define WINDOWSETTINGSHELPER_H

#include <QWidget>
#include <QString>
#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QPalette>
#include <QDebug>
#include <QTableView>
#include <QHeaderView>
#include <QMetaObject>
#include <QList>

class WindowSettingsHelper {
public:
    static void saveWindowSettings(QWidget* window, const QString& windowType) {
        if (!window) return;
        
        QSettings settings("TradingCompany", "TradingTerminal");
        settings.beginGroup(QString("WindowState/%1").arg(windowType));
        
        // Save Geometry if the parent is a CustomMDISubWindow
        QWidget* subWin = window->parentWidget();
        while (subWin && !subWin->inherits("CustomMDISubWindow")) {
            subWin = subWin->parentWidget();
        }
        if (subWin) {
            QRect geom = subWin->geometry();
            settings.setValue("x", geom.x());
            settings.setValue("y", geom.y());
            settings.setValue("width", geom.width());
            settings.setValue("height", geom.height());
        }

        // Find all ComboBoxes and save their current text
        QList<QComboBox*> combos = window->findChildren<QComboBox*>();
        for (QComboBox* cb : combos) {
            if (!cb->objectName().isEmpty() && !cb->objectName().startsWith("qt_")) {
                settings.setValue(QString("combo/%1").arg(cb->objectName()), cb->currentText());
            }
        }
        
        // Find all CheckBoxes and save their state
        QList<QCheckBox*> checks = window->findChildren<QCheckBox*>();
        for (QCheckBox* ck : checks) {
            if (!ck->objectName().isEmpty() && !ck->objectName().startsWith("qt_")) {
                settings.setValue(QString("check/%1").arg(ck->objectName()), ck->isChecked());
            }
        }
        
        settings.endGroup();
        qDebug() << "[WindowSettingsHelper] Saved runtime state for" << windowType;
    }

    static void loadAndApplyWindowSettings(QWidget* window, const QString& windowType) {
        if (!window) return;
        
        // 1. Apply Customization (Colors, Fonts)
        applyCustomization(window, windowType);
        
        // 2. Apply Runtime State (Filters, Checkboxes, Geometry)
        QSettings settings("TradingCompany", "TradingTerminal");
        settings.beginGroup(QString("WindowState/%1").arg(windowType));
        
        // Restore Geometry — always restore last-saved position/size
        {
            QWidget* subWin = window->parentWidget();
            while (subWin && !subWin->inherits("CustomMDISubWindow")) {
                subWin = subWin->parentWidget();
            }
            if (subWin) {
                // Use individual x/y/width/height keys (reliable for child widgets)
                bool hasX = settings.contains("x");
                bool hasY = settings.contains("y");
                bool hasW = settings.contains("width");
                bool hasH = settings.contains("height");

                if (hasX && hasY && hasW && hasH) {
                    int x = settings.value("x").toInt();
                    int y = settings.value("y").toInt();
                    int w = settings.value("width").toInt();
                    int h = settings.value("height").toInt();
                    if (w > 0 && h > 0) {
                        subWin->setGeometry(x, y, w, h);
                        qDebug() << "[WindowSettingsHelper] Restored geometry for"
                                 << windowType << "-> (" << x << y << w << h << ")";
                    }
                } else {
                    // Legacy fallback: try old QByteArray key
                    QByteArray geom = settings.value("geometry").toByteArray();
                    if (!geom.isEmpty()) {
                        subWin->restoreGeometry(geom);
                        qDebug() << "[WindowSettingsHelper] Restored legacy geometry for" << windowType;
                    }
                }
            }
        }

        QList<QComboBox*> combos = window->findChildren<QComboBox*>();
        for (QComboBox* cb : combos) {
            if (!cb->objectName().isEmpty()) {
                QString val = settings.value(QString("combo/%1").arg(cb->objectName())).toString();
                if (!val.isEmpty()) {
                    int index = cb->findText(val);
                    if (index >= 0) {
                        cb->setCurrentIndex(index);
                        // Trigger any connected slots
                        emit cb->currentTextChanged(val);
                    }
                }
            }
        }
        
        QList<QCheckBox*> checks = window->findChildren<QCheckBox*>();
        for (QCheckBox* ck : checks) {
            if (!ck->objectName().isEmpty()) {
                bool val = settings.value(QString("check/%1").arg(ck->objectName()), ck->isChecked()).toBool();
                ck->setChecked(val);
                // Trigger any connected slots
                emit ck->toggled(val);
            }
        }
        
        settings.endGroup();
        qDebug() << "[WindowSettingsHelper] Loaded and applied runtime state for" << windowType;
    }

    static void applyCustomization(QWidget* window, const QString& windowType) {
        QSettings settings("TradingCompany", "TradingTerminal");
        settings.beginGroup(QString("Customize/%1").arg(windowType));
        
        // ── Read legacy keys (also written by the new dialog for compat) ──
        QString bgColor = settings.value("backgroundColor", "").toString();
        QString fgColor = settings.value("foregroundColor", "").toString();

        // ── Font settings (new: fontSize, fontBold, fontItalic) ──
        QString fontName = settings.value("font", "").toString();
        int     fontSize = settings.value("fontSize", 0).toInt();
        bool    fontBold = settings.value("fontBold", false).toBool();
        bool    fontItalic = settings.value("fontItalic", false).toBool();

        // ── Layout toggles ──
        bool titleBarVisible = settings.value("titleBar", true).toBool();
        bool toolBarVisible  = settings.value("toolBar", true).toBool();
        bool gridVisible     = settings.value("grid", true).toBool();
        
        // ── Per-attribute colors (new) ──
        QString selBg     = settings.value("color/Selection/Background", "").toString();
        QString selFg     = settings.value("color/Selection/Foreground", "").toString();
        QString hdrBg     = settings.value("color/Table Header/Background", "").toString();
        QString hdrFg     = settings.value("color/Table Header/Foreground", "").toString();
        QString gridColor = settings.value("color/Grid Lines/Foreground", "").toString();
        QString evenRowBg = settings.value("color/Table Row (Even)/Background", "").toString();
        QString oddRowBg  = settings.value("color/Table Row (Odd)/Background", "").toString();
        QString upColor   = settings.value("color/Positive \\/ Up/Foreground", "").toString();
        QString downColor = settings.value("color/Negative \\/ Down/Foreground", "").toString();

        // Handle TitleBar visibility if parent is CustomMDISubWindow
        QWidget* subWin = window->parentWidget();
        while (subWin && !subWin->inherits("CustomMDISubWindow")) {
            subWin = subWin->parentWidget();
        }
        if (subWin) {
            subWin->setProperty("titleBarVisible", titleBarVisible);
            QMetaObject::invokeMethod(subWin, "updateTitleBarVisibility");
        }

        // Apply background/foreground styling
        if (!bgColor.isEmpty() || !fgColor.isEmpty() || !fontName.isEmpty()) {
            QWidget *widgetToStyle = window;
            QList<QWidget*> children = window->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
            for (QWidget* child : children) {
                if (child->metaObject()->className() == QString("QWidget") && child->layout() != nullptr) {
                    widgetToStyle = child;
                    break;
                }
            }

            widgetToStyle->setAttribute(Qt::WA_StyledBackground, true);
            
            QString widgetClassName = widgetToStyle->metaObject()->className();
            QString style = QString("%1 { ").arg(widgetClassName);
            if (!bgColor.isEmpty()) style += QString("background-color: %1; ").arg(bgColor);
            if (!fgColor.isEmpty()) style += QString("color: %1; ").arg(fgColor);
            style += "} ";
            
            style += " QLineEdit, QComboBox { background-color: white; color: black; border: 1px solid #ccc; }";
            style += " QLabel { background-color: transparent; }";
            
            // ── Per-attribute table styling ──
            // Grid
            if (!gridVisible) {
                style += " QTableView { gridline-color: transparent; }";
            } else if (!gridColor.isEmpty()) {
                style += QString(" QTableView { gridline-color: %1; }").arg(gridColor);
            } else {
                style += " QTableView { gridline-color: #d4d4d8; }";
            }

            // Header
            if (!hdrBg.isEmpty() || !hdrFg.isEmpty()) {
                style += " QHeaderView::section { ";
                if (!hdrBg.isEmpty()) style += QString("background-color: %1; ").arg(hdrBg);
                if (!hdrFg.isEmpty()) style += QString("color: %1; ").arg(hdrFg);
                style += "} ";
            }

            // Selection
            if (!selBg.isEmpty() || !selFg.isEmpty()) {
                style += " QTableView::item:selected { ";
                if (!selBg.isEmpty()) style += QString("background-color: %1; ").arg(selBg);
                if (!selFg.isEmpty()) style += QString("color: %1; ").arg(selFg);
                style += "} ";
            }

            // Alternating row colors
            if (!evenRowBg.isEmpty() || !oddRowBg.isEmpty()) {
                if (!evenRowBg.isEmpty())
                    style += QString(" QTableView { alternate-background-color: %1; }").arg(oddRowBg.isEmpty() ? evenRowBg : oddRowBg);
            }

            widgetToStyle->setStyleSheet(style);
        }

        // ── Apply Font (family + size + bold + italic) ──
        if (!fontName.isEmpty()) {
            QFont f = window->font();
            f.setFamily(fontName);
            if (fontSize > 0) f.setPointSize(fontSize);
            f.setBold(fontBold);
            f.setItalic(fontItalic);
            window->setFont(f);
            for (QWidget* child : window->findChildren<QWidget*>()) {
                child->setFont(f);
            }
        }
        
        // Handle explicit Grid toggle for QTableView children
        QList<QTableView*> tables = window->findChildren<QTableView*>();
        for (QTableView* table : tables) {
            table->setShowGrid(gridVisible);
        }

        settings.endGroup();
    }
};

#endif // WINDOWSETTINGSHELPER_H
