#ifndef STRATEGY_DEPLOY_DIALOG_H
#define STRATEGY_DEPLOY_DIALOG_H

/**
 * @file StrategyDeployDialog.h
 * @brief Phase 3 – Template Deploy Dialog
 *
 * Workflow:
 *  1. "Pick Template"  tab  – Browse / search all saved templates, select one.
 *  2. "Bind Symbols"   tab  – For each SymbolDefinition in the template, the
 *                             user types a symbol name + segment, and the widget
 *                             resolves it to a real ContractData token.
 *  3. "Parameters"     tab  – Fill all TemplateParam values (pre-filled with
 *                             template defaults).  Locked params are greyed out.
 *  4. "Risk & Limits"  tab  – Override stop-loss %, target %, trailing,
 *                             time-exit, and per-day limits.  Locked fields are
 *                             greyed out.
 *
 * On Accept the caller can retrieve:
 *   - The selected StrategyTemplate       via selectedTemplate()
 *   - The resolved symbol bindings        via symbolBindings()
 *   - The final parameter values          via paramValues()
 *   - The final risk overrides            via riskOverride()
 *   - A ready-to-save StrategyInstance    via buildInstance()
 */

#include "models/StrategyInstance.h"
#include "repository/ContractData.h"
#include "repository/RepositoryManager.h"
#include "strategy/StrategyTemplate.h"
#include <QDialog>
#include <QMap>
#include <QTableWidget>
#include <QVector>

class QTabWidget;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QTableWidget;
class QPushButton;
class QComboBox;
class QFormLayout;
class QGroupBox;

// ─────────────────────────────────────────────────────────────────────────────
// Helper widget: one row for binding a SymbolDefinition slot
// ─────────────────────────────────────────────────────────────────────────────
class SymbolBindingRow : public QWidget {
    Q_OBJECT
public:
    explicit SymbolBindingRow(const SymbolDefinition &def, QWidget *parent = nullptr);

    /// Returns true when a valid ContractData has been resolved for this slot.
    bool isResolved() const { return m_resolved; }

    /// Returns the filled-in SymbolBinding (valid only when isResolved()).
    SymbolBinding binding() const { return m_binding; }

signals:
    void bindingResolved(const QString &symbolId);
    void bindingCleared(const QString &symbolId);

private slots:
    void onSearchClicked();
    void onClearClicked();
    void onInlineSearch(const QString &text);
    void onInlineEnter();

private:
    void setupUI(const SymbolDefinition &def);
    void applyContract(const ContractData &c);
    void pickInlineRow(int row);

    SymbolDefinition m_def;
    SymbolBinding    m_binding;
    bool             m_resolved = false;

    QLineEdit    *m_nameEdit       = nullptr;
    QLabel       *m_tokenLabel     = nullptr;
    QPushButton  *m_searchBtn      = nullptr;
    QPushButton  *m_clearBtn       = nullptr;
    QSpinBox     *m_qtySpinBox     = nullptr;
    QTableWidget *m_inlineResults  = nullptr;

    QVector<ContractData> m_inlineContracts;
};

// ─────────────────────────────────────────────────────────────────────────────
// Main dialog
// ─────────────────────────────────────────────────────────────────────────────
class StrategyDeployDialog : public QDialog {
    Q_OBJECT

public:
    explicit StrategyDeployDialog(QWidget *parent = nullptr);
    ~StrategyDeployDialog() override = default;

    // ── Outputs ──────────────────────────────────────────────────────────────
    StrategyTemplate    selectedTemplate()  const { return m_template; }
    QVector<SymbolBinding> symbolBindings() const;
    QVariantMap         paramValues()       const;
    RiskDefaults        riskOverride()      const;

    /// Builds a fully-populated StrategyInstance ready for StrategyService.
    StrategyInstance    buildInstance()     const;

private slots:
    void onTemplateSelectionChanged();
    void onTemplateDoubleClicked();
    void onNextClicked();
    void onBackClicked();
    void onDeployClicked();
    void onBindingResolved(const QString &symbolId);
    void onBindingCleared(const QString &symbolId);

private:
    // ── Page builders ────────────────────────────────────────────────────────
    QWidget *buildPickTemplatePage();
    QWidget *buildBindSymbolsPage();
    QWidget *buildParametersPage();
    QWidget *buildRiskPage();

    // ── Helpers ──────────────────────────────────────────────────────────────
    void loadTemplates();
    void populateSymbolPage();
    void populateParamsPage();
    void populateRiskPage();

    bool validateCurrentPage();
    void updateNavigationButtons();
    void goToPage(int index);

    // ── UI elements ──────────────────────────────────────────────────────────
    QTabWidget  *m_tabs        = nullptr;
    QPushButton *m_backBtn     = nullptr;
    QPushButton *m_nextBtn     = nullptr;
    QPushButton *m_deployBtn   = nullptr;
    QPushButton *m_cancelBtn   = nullptr;

    // Page 0 – Template picker
    QTableWidget *m_templateTable = nullptr;
    QLabel       *m_templateDesc  = nullptr;
    QLabel       *m_templateMeta  = nullptr;

    // Page 1 – Symbol binding rows
    QWidget                    *m_symbolPage      = nullptr;
    QVector<SymbolBindingRow*>  m_bindingRows;

    // Page 2 – Parameters
    QWidget                           *m_paramsPage   = nullptr;
    QMap<QString, QWidget*>            m_paramEditors; // name → editor widget

    // Page 3 – Risk
    QWidget        *m_riskPage            = nullptr;
    QDoubleSpinBox *m_slPctSpin           = nullptr;
    QCheckBox      *m_slLockedCheck       = nullptr;
    QDoubleSpinBox *m_tgtPctSpin          = nullptr;
    QCheckBox      *m_tgtLockedCheck      = nullptr;
    QCheckBox      *m_trailingCheck       = nullptr;
    QDoubleSpinBox *m_trailTriggerSpin    = nullptr;
    QDoubleSpinBox *m_trailAmountSpin     = nullptr;
    QCheckBox      *m_timeExitCheck       = nullptr;
    QLineEdit      *m_timeExitEdit        = nullptr;
    QSpinBox       *m_maxTradesSpin       = nullptr;
    QDoubleSpinBox *m_maxDailyLossSpin    = nullptr;

    // Instance name
    QLineEdit   *m_instanceNameEdit   = nullptr;
    QLineEdit   *m_instanceDescEdit   = nullptr;
    QComboBox   *m_accountCombo       = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    int              m_currentPage = 0;
    StrategyTemplate m_template;
    QVector<StrategyTemplate> m_allTemplates;
};

#endif // STRATEGY_DEPLOY_DIALOG_H
