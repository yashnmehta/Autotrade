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

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;

class TemplateManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit TemplateManagerDialog(QWidget *parent = nullptr);
    ~TemplateManagerDialog() override = default;

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
    void setupUI();
    void loadTemplates();
    void updateDetailPane();
    void updateButtonStates();
    int  selectedRow() const;
    void filterTable(const QString &text);

    // ── UI elements ──
    QLineEdit    *m_searchEdit     = nullptr;
    QTableWidget *m_templateTable  = nullptr;
    QLabel       *m_detailTitle    = nullptr;
    QLabel       *m_detailMeta     = nullptr;
    QTextEdit    *m_detailDesc     = nullptr;
    QLabel       *m_detailSymbols  = nullptr;
    QLabel       *m_detailParams   = nullptr;
    QLabel       *m_detailIndicators = nullptr;
    QLabel       *m_detailRisk     = nullptr;

    QPushButton  *m_createBtn      = nullptr;
    QPushButton  *m_editBtn        = nullptr;
    QPushButton  *m_cloneBtn       = nullptr;
    QPushButton  *m_deleteBtn      = nullptr;
    QPushButton  *m_deployBtn      = nullptr;
    QPushButton  *m_closeBtn       = nullptr;

    // ── Data ──
    QVector<StrategyTemplate> m_allTemplates;
    StrategyTemplate          m_selectedTemplate;
    Action                    m_resultAction = Action::None;
};

#endif // TEMPLATE_MANAGER_DIALOG_H
