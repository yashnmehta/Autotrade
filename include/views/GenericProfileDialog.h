#ifndef GENERICPROFILEDIALOG_H
#define GENERICPROFILEDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include "models/GenericTableProfile.h"
#include "models/GenericProfileManager.h"

class GenericProfileDialog : public QDialog
{
    Q_OBJECT

public:
    GenericProfileDialog(const QString &windowName, 
                         const QList<GenericColumnInfo> &allColumns, 
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
    
    QString m_windowName;
    QList<GenericColumnInfo> m_allColumns;
    GenericTableProfile m_profile;
    GenericProfileManager m_manager;
    bool m_accepted;

    QComboBox *m_profileCombo;
    QListWidget *m_availableList;
    QListWidget *m_selectedList;
    QPushButton *m_addButton, *m_removeButton, *m_moveUpButton, *m_moveDownButton;
    QPushButton *m_saveButton, *m_deleteButton, *m_setDefaultButton, *m_okButton, *m_cancelButton;
};

#endif // GENERICPROFILEDIALOG_H
