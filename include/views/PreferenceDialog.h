#ifndef PREFERENCEDIALOG_H
#define PREFERENCEDIALOG_H

#include <QDialog>
#include <QShortcut>

namespace Ui {
class PreferencesWindowTab;
}

class PreferencesManager;
class PreferencesGeneralTab;

class PreferenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferenceDialog(QWidget *parent = nullptr);
    ~PreferenceDialog();

private slots:
    void onApplyClicked();
    void onOkClicked();
    void onRestoreDefaultsClicked();
    void onBrowseClicked();

private:
    void loadPreferences();
    void savePreferences();
    void setupConnections();
    void applyPreferences();
    void loadTabContent(const QString &tabName, const QString &uiFilePath);

    Ui::PreferencesWindowTab *ui;
    PreferencesManager *m_prefsManager;
    QShortcut* m_escShortcut;
    
    // Tab handlers
    PreferencesGeneralTab *m_generalTab;
};

#endif // PREFERENCEDIALOG_H
