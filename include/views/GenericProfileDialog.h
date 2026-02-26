#ifndef GENERICPROFILEDIALOG_H
#define GENERICPROFILEDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include "models/GenericTableProfile.h"
#include "models/GenericProfileManager.h"

/**
 * @brief Universal dual-list column profile dialog.
 *
 * Works for any window type — MarketWatch, OptionChain, OrderBook, etc.
 * The caller provides:
 *   - a window name (for file namespacing)
 *   - the full list of available columns (GenericColumnInfo)
 *   - a pointer to the GenericProfileManager (owns presets + custom)
 *   - the currently active profile
 *
 * The dialog handles preset/custom profile switching, add/remove/reorder
 * columns, save/delete custom profiles, and returns the accepted profile.
 */
class GenericProfileDialog : public QDialog
{
    Q_OBJECT

public:
    GenericProfileDialog(const QString &windowTitle,
                         const QList<GenericColumnInfo> &allColumns,
                         GenericProfileManager *manager,
                         const GenericTableProfile &currentProfile,
                         QWidget *parent = nullptr);

    GenericTableProfile getProfile() const { return m_profile; }
    bool wasAccepted() const { return m_accepted; }

private slots:
    void onProfileChanged(const QString &profileName);
    void onAddColumn();
    void onRemoveColumn();
    void onMoveUp();
    void onMoveDown();
    void onSaveProfile();
    void onDeleteProfile();
    void onSetAsDefault();
    void onAccepted();
    void onRejected();

private:
    void setupUI();
    void setupConnections();
    void loadProfile(const GenericTableProfile &profile);
    void updateAvailableColumns();
    void updateSelectedColumns();
    void refreshProfileList();
    QString columnNameById(int id) const;

    QList<GenericColumnInfo> m_allColumns;
    GenericProfileManager *m_manager;          ///< NOT owned — caller keeps alive
    GenericTableProfile m_profile;
    bool m_accepted;

    QComboBox *m_profileCombo;
    QListWidget *m_availableList;
    QListWidget *m_selectedList;
    QPushButton *m_addButton, *m_removeButton, *m_moveUpButton, *m_moveDownButton;
    QPushButton *m_saveButton, *m_deleteButton, *m_setDefaultButton, *m_okButton, *m_cancelButton;
};

#endif // GENERICPROFILEDIALOG_H
