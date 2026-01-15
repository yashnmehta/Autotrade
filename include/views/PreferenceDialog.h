#ifndef PREFERENCEDIALOG_H
#define PREFERENCEDIALOG_H

#include <QDialog>
#include <QShortcut>

namespace Ui {
class PreferencesWindowTab;
}

class PreferencesManager;
class PreferencesGeneralTab;
class PreferencesOrderTab;

class PreferenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferenceDialog(QWidget *parent = nullptr);
    ~PreferenceDialog();

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onRestoreDefaultsClicked();
    void onBrowseClicked();

private:
    void loadTabContent(const QString &tabName, const QString &uiFilePath);
    void setupConnections();
    void loadPreferences();
    void savePreferences();
    void applyPreferences();

    Ui::PreferencesWindowTab *ui;
    PreferencesManager *m_prefsManager;
    
    // Tab handlers
    PreferencesGeneralTab *m_generalTab;
    PreferencesOrderTab *m_orderTab;
};

#endif // PREFERENCEDIALOG_H
