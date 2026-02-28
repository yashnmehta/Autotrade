#ifndef TABLEPROFILEHELPER_H
#define TABLEPROFILEHELPER_H

#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemModel>
#include "models/profiles/GenericTableProfile.h"
#include "models/profiles/GenericProfileManager.h"
#include "views/GenericProfileDialog.h"

class TableProfileHelper {
public:
    static void loadProfile(const QString& windowName, QTableView* tableView, QAbstractItemModel* model, GenericTableProfile& profile) {
        GenericProfileManager manager("profiles", windowName);
        manager.loadCustomProfiles();

        // Priority: LastUsed file > named default/custom > built-in default
        GenericTableProfile lastUsed;
        if (manager.loadLastUsedProfile(lastUsed)) {
            profile = lastUsed;
        } else {
            QString defName = manager.loadDefaultProfileName();
            if (manager.hasProfile(defName)) {
                profile = manager.getProfile(defName);
            } else {
                profile = GenericTableProfile("Default");
                QList<int> defaultOrder;
                for (int i = 0; i < model->columnCount(); ++i) {
                    profile.setColumnVisible(i, true);
                    profile.setColumnWidth(i, 100);
                    defaultOrder.append(i);
                }
                profile.setColumnOrder(defaultOrder);
            }
        }
        applyProfile(tableView, model, profile);
    }

    static void applyProfile(QTableView* tableView, QAbstractItemModel* model, const GenericTableProfile& profile) {
        QHeaderView* header = tableView->horizontalHeader();
        QList<int> order = profile.columnOrder();
        
        // First hide/show and set widths
        for (int i = 0; i < model->columnCount(); ++i) {
            bool visible = profile.isColumnVisible(i);
            tableView->setColumnHidden(i, !visible);
            if (visible) {
                tableView->setColumnWidth(i, profile.columnWidth(i));
            }
        }
        
        // Then apply visual order
        if (!order.isEmpty()) {
            for (int i = 0; i < order.size(); ++i) {
                int logicalIdx = order[i];
                if (logicalIdx >= 0 && logicalIdx < model->columnCount()) {
                    int currentVisualIdx = header->visualIndex(logicalIdx);
                    if (currentVisualIdx != i) {
                        header->moveSection(currentVisualIdx, i);
                    }
                }
            }
        }
    }

    static void captureProfile(QTableView* tableView, QAbstractItemModel* model, GenericTableProfile& profile) {
        QHeaderView* header = tableView->horizontalHeader();
        QList<int> currentOrder;
        
        for (int i = 0; i < model->columnCount(); ++i) {
            // Capture visual order
            currentOrder.append(header->logicalIndex(i));
            
            // Capture width if column is visible (or even if not, if we can)
            if (!tableView->isColumnHidden(i)) {
                profile.setColumnWidth(i, tableView->columnWidth(i));
            }
        }
        profile.setColumnOrder(currentOrder);
    }

    static void saveCurrentProfile(const QString& windowName, QTableView* tableView, QAbstractItemModel* model, GenericTableProfile& profile) {
        captureProfile(tableView, model, profile);
        GenericProfileManager manager("profiles", windowName);
        manager.saveLastUsedProfile(profile);          // always works, even for preset names
        manager.saveCustomProfile(profile);             // also save as custom if not a preset
        manager.saveDefaultProfileName(profile.name());
    }

    static bool showProfileDialog(const QString& windowName, QTableView* tableView, QAbstractItemModel* model, GenericTableProfile& profile, QWidget* parent) {
        // Sync current table state (widths/order) to profile before opening dialog
        captureProfile(tableView, model, profile);
        
        QList<GenericColumnInfo> allColumns;
        for (int i = 0; i < model->columnCount(); ++i) {
            GenericColumnInfo info;
            info.id = i;
            info.name = model->headerData(i, Qt::Horizontal).toString();
            info.defaultWidth = 100;
            info.visibleByDefault = true;
            allColumns.append(info);
        }

        GenericProfileManager manager("profiles", windowName);
        manager.loadCustomProfiles();

        GenericProfileDialog dialog(windowName, allColumns, &manager, profile, parent);
        if (dialog.exec() == QDialog::Accepted) {
            profile = dialog.getProfile();
            applyProfile(tableView, model, profile);
            
            // Save as default
            manager.saveLastUsedProfile(profile);          // always works, even for preset names
            manager.saveCustomProfile(profile);             // also save as custom if not a preset
            manager.saveDefaultProfileName(profile.name());
            return true;
        }
        return false;
    }
};

#endif // TABLEPROFILEHELPER_H
