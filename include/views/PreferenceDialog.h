#ifndef PREFERENCEDIALOG_H
#define PREFERENCEDIALOG_H

#include <QDialog>
#include <QShortcut>

namespace Ui {
class PreferenceDialog;
}

class PreferencesManager;

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

    Ui::PreferenceDialog *ui;
    PreferencesManager *m_prefsManager;
    QShortcut* m_escShortcut;
};

#endif // PREFERENCEDIALOG_H
