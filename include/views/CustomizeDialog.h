#ifndef CUSTOMIZEDIALOG_H
#define CUSTOMIZEDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class CustomizeDialog;
}

class CustomizeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomizeDialog(const QString &windowType, QWidget *targetWindow, QWidget *parent = nullptr);
    ~CustomizeDialog();

private slots:
    void onApplyClicked();
    void onColorPickerClicked();
    void onResetClicked();

private:
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void applySettings();

    Ui::CustomizeDialog *ui;
    QString m_windowType;
    QWidget *m_targetWindow;
};

#endif // CUSTOMIZEDIALOG_H
