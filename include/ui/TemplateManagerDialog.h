#ifndef TEMPLATEMANAGERDIALOG_H
#define TEMPLATEMANAGERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

class TradingViewChartWidget;

/**
 * @brief Dialog for managing TradingView chart templates
 * 
 * Provides UI for:
 * - Saving current chart configuration as a template
 * - Loading existing templates
 * - Deleting templates
 * - Viewing template list
 */
class TemplateManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit TemplateManagerDialog(TradingViewChartWidget* chartWidget, QWidget* parent = nullptr);

private slots:
    void onSaveTemplate();
    void onLoadTemplate();
    void onDeleteTemplate();
    void onTemplateSelected();
    void refreshTemplateList();

private:
    void setupUi();

    TradingViewChartWidget* m_chartWidget;
    QListWidget* m_templateList;
    QLineEdit* m_templateNameEdit;
    QPushButton* m_saveButton;
    QPushButton* m_loadButton;
    QPushButton* m_deleteButton;
    QLabel* m_statusLabel;
};

#endif // TEMPLATEMANAGERDIALOG_H
