#ifndef TEMPLATE_MANAGER_DIALOG_H
#define TEMPLATE_MANAGER_DIALOG_H

/**
 * @file TemplateManagerDialog.h
 * @brief Unified Template Management Dialog
 *
 * Replaces the separate "Build Custom" + "Deploy Template" workflow with a
 * single dialog that supports:
 *   - Browse all saved templates (list + detail pane)
 *   - Create new template (opens StrategyTemplateBuilderDialog)
 *   - Edit existing template (opens builder pre-filled)
 *   - Clone a template
 *   - Delete a template (soft-delete)
 *   - Deploy a template (opens StrategyDeployDialog with pre-selected template)
 *
 * The dialog itself is non-modal by default and can remain open while other
 * actions occur.
 */

#include "strategy/model/StrategyTemplate.h"
#include <QDialog>
#include <QVector>

namespace Ui { class TemplateManagerDialog; }

class TemplateManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit TemplateManagerDialog(QWidget *parent = nullptr);
    ~TemplateManagerDialog() override;

    /// After Accepted with action == Deploy, call this to get the template.
    StrategyTemplate selectedTemplate() const { return m_selectedTemplate; }

    enum class Action { None, Deploy };
    Action resultAction() const { return m_resultAction; }

signals:
    /// Emitted when the user clicks Deploy on a template.
    void deployRequested(const StrategyTemplate &tmpl);

private slots:
    void onSelectionChanged();
    void onDoubleClicked();
    void onSearchChanged(const QString &text);

    void onCreateClicked();
    void onEditClicked();
    void onCloneClicked();
    void onDeleteClicked();
    void onDeployClicked();

private:
    void initUI();
    void loadTemplates();
    void updateDetailPane();
    void updateButtonStates();
    int  selectedRow() const;
    void filterTable(const QString &text);

    // ── UI ──
    Ui::TemplateManagerDialog *ui;

    // ── Data ──
    QVector<StrategyTemplate> m_allTemplates;
    StrategyTemplate          m_selectedTemplate;
    Action                    m_resultAction = Action::None;
};

#endif // TEMPLATE_MANAGER_DIALOG_H
