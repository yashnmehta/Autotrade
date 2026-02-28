#ifndef CUSTOMIZEDIALOG_H
#define CUSTOMIZEDIALOG_H

#include <QDialog>
#include <QString>
#include <QColor>
#include <QFont>
#include <QMap>

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
    void onAttributeSelected(int row);
    void onColorForChanged(int index);
    void onFontChanged();
    void onFontSizeChanged(int size);
    void onFontStyleChanged();
    void onViewChanged(int index);

private:
    void setupConnections();
    void populateAttributeList();
    void populateViewComboBox();
    void loadSettings();
    void saveSettings();
    void applySettings();
    void updateColorPreview();
    void updateFontPreview();
    void updateColorSwatchButton();

    // Color attribute helpers
    QString currentAttributeKey() const;
    QColor  colorForAttribute(const QString &attrKey, const QString &colorType) const;
    void    setColorForAttribute(const QString &attrKey, const QString &colorType, const QColor &color);

    Ui::CustomizeDialog *ui;
    QString m_windowType;
    QWidget *m_targetWindow;

    // Per-attribute color map: key = "attr/colorType" -> QColor
    // e.g. "General Appearance/Background" -> #ffffff
    QMap<QString, QColor> m_colorMap;

    // All known window types for the "View" combo
    static const QStringList s_allWindowTypes;
    // All color attributes
    static const QStringList s_colorAttributes;
};

#endif // CUSTOMIZEDIALOG_H
