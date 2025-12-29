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
            settings.setValue("geometry", subWin->saveGeometry());
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
        
        // Restore Geometry if openWith is sizePosition or lastSaved
        QSettings custSettings("TradingCompany", "TradingTerminal");
        QString openWith = custSettings.value(QString("Customize/%1/openWith").arg(windowType), "default").toString();
        
        if (openWith != "default") {
            QWidget* subWin = window->parentWidget();
            while (subWin && !subWin->inherits("CustomMDISubWindow")) {
                subWin = subWin->parentWidget();
            }
            if (subWin) {
                QByteArray geom = settings.value("geometry").toByteArray();
                if (!geom.isEmpty()) {
                    subWin->restoreGeometry(geom);
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
        
        QString bgColor = settings.value("backgroundColor", "").toString();
        QString fgColor = settings.value("foregroundColor", "").toString();
        QString fontName = settings.value("font", "").toString();
        bool titleBarVisible = settings.value("titleBar", true).toBool();
        bool toolBarVisible = settings.value("toolBar", true).toBool();
        bool gridVisible = settings.value("grid", true).toBool();
        
        // Handle TitleBar visibility if parent is CustomMDISubWindow
        QWidget* subWin = window->parentWidget();
        while (subWin && !subWin->inherits("CustomMDISubWindow")) {
            subWin = subWin->parentWidget();
        }
        if (subWin) {
            // Use meta-method or direct call if possible. 
            // Since we might not have the header here, we can set property and let it handle
            subWin->setProperty("titleBarVisible", titleBarVisible);
            // Trigger update if it's a known method
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
            
            // Handle Grid visibility for table views if applicable
            if (!gridVisible) {
                style += " QTableView { gridline-color: transparent; }";
            } else {
                style += " QTableView { gridline-color: #d4d4d8; }";
            }
            
            widgetToStyle->setStyleSheet(style);
            
            // Apply Font
            if (!fontName.isEmpty()) {
                QFont f = window->font();
                f.setFamily(fontName);
                window->setFont(f);
                for (QWidget* child : window->findChildren<QWidget*>()) {
                    child->setFont(f);
                }
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
