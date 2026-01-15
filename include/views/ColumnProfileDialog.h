#ifndef COLUMNPROFILEDIALOG_H
#define COLUMNPROFILEDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include "models/MarketWatchColumnProfile.h"

/**
 * @brief Dialog for managing market watch column profiles
 * 
 * Features:
 * - Select from predefined or custom profiles
 * - Add/Remove columns
 * - Reorder columns (Move Up/Down)
 * - Save/Delete profiles
 * - Set default profile
 */
class ColumnProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColumnProfileDialog(const MarketWatchColumnProfile &currentProfile, ProfileContext context = ProfileContext::MarketWatch, QWidget *parent = nullptr);
    
    /**
     * @brief Get the configured profile
     * @return Modified column profile
     */
    MarketWatchColumnProfile getProfile() const { return m_profile; }
    
    /**
     * @brief Check if user accepted changes
     */
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
    void loadProfile(const MarketWatchColumnProfile &profile);
    void updateAvailableColumns();
    void updateSelectedColumns();
    void refreshProfileList();
    
    // UI Components
    QComboBox *m_profileCombo;
    QListWidget *m_availableList;
    QListWidget *m_selectedList;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QPushButton *m_saveButton;
    QPushButton *m_deleteButton;
    QPushButton *m_setDefaultButton;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    
    // Data
    MarketWatchColumnProfile m_profile;
    ProfileContext m_context;
    bool m_accepted;
};

#endif // COLUMNPROFILEDIALOG_H
