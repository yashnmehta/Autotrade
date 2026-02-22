#ifndef STRATEGY_TEMPLATE_BUILDER_DIALOG_H
#define STRATEGY_TEMPLATE_BUILDER_DIALOG_H

#include "strategy/StrategyTemplate.h"
#include <QDialog>
#include <QList>

class QComboBox;
class QSpinBox;

class ConditionBuilderWidget;
class IndicatorRowWidget;
class QVBoxLayout;

namespace Ui {
class StrategyTemplateBuilderDialog;
}

/**
 * @brief Multi-tab dialog for creating or editing a StrategyTemplate.
 *
 * Tabs (defined in StrategyTemplateBuilderDialog.ui):
 *   1. Metadata    — name, description, mode, flags
 *   2. Symbols     — declare symbol slots (REF / TRADE, type)
 *   3. Indicators  — declare indicators per symbol slot
 *   4. Parameters  — user-configurable parameter declarations
 *   5. Conditions  — entry / exit condition trees (ConditionBuilderWidget promoted)
 *   6. Risk        — stop-loss, target, trailing, time exit, daily limits
 *
 * Usage:
 *   StrategyTemplateBuilderDialog dlg(this);
 *   dlg.setTemplate(existingTemplate);          // edit mode (optional)
 *   if (dlg.exec() == QDialog::Accepted)
 *       StrategyTemplate tmpl = dlg.buildTemplate();
 */
class StrategyTemplateBuilderDialog : public QDialog {
    Q_OBJECT

public:
    explicit StrategyTemplateBuilderDialog(QWidget *parent = nullptr);
    ~StrategyTemplateBuilderDialog() override;

    /// Pre-fill all tabs with an existing template (for editing)
    void setTemplate(const StrategyTemplate &tmpl);

    /// Returns the template built from the current form state.
    /// Only valid after exec() == Accepted.
    StrategyTemplate buildTemplate() const;

protected:
    void accept() override;

private:
    // ── Populate helpers (edit mode) ──
    void populateMetadata(const StrategyTemplate &tmpl);
    void populateSymbols(const StrategyTemplate &tmpl);
    void populateIndicators(const StrategyTemplate &tmpl);
    void populateParameters(const StrategyTemplate &tmpl);
    void populateConditions(const StrategyTemplate &tmpl);
    void populateRisk(const StrategyTemplate &tmpl);

    // ── Extract helpers ──
    QVector<SymbolDefinition>     extractSymbols() const;
    QVector<IndicatorDefinition>  extractIndicators() const;
    QVector<TemplateParam>        extractParams() const;
    RiskDefaults                  extractRisk() const;

    // ── Context refresh (updates condition widget dropdowns) ──
    void refreshConditionContext();

    // ── Symbols table helpers ──
    int  addSymbolRow(const QString &id, const QString &label,
                      int roleIndex, int segmentIndex);
    void addDefaultSymbolSlots();

    // ── Symbols tab slots ──
    void onAddSymbol();
    void onRemoveSymbol();
    void onSymbolTableChanged();

    // ── Indicators tab (card-based) ──
    IndicatorRowWidget *addIndicatorCard(const IndicatorDefinition &ind = {});
    void removeIndicatorCard(IndicatorRowWidget *card);
    void loadIndicatorCatalog();
    // Compatibility shim — converts old field-by-field call into an addIndicatorCard call
    int  addIndicatorRow(const QString &id, const QString &type,
                         const QString &symbolId, const QString &period1,
                         const QString &period2,  const QString &priceField,
                         const QString &param3 = {},
                         const QString &outputSel = {},
                         const QString &timeframe = {});

    // ── Indicators tab slot ──
    void onAddIndicator();

    // ── Parameters tab slots ──
    void onAddParam();
    void onEditParam();           // Edit button clicked (edits selected row)
    void onEditParamRow(int row); // Opens ParamEditorDialog for a specific row
    void onRemoveParam();

    // ── Parameter summary table helper ──
    // Rebuilds the read-only summary table from m_params.
    void refreshParamsTable();

    // ── Validation ──
    bool validate();

    // ── UI (generated from .ui file) ──
    Ui::StrategyTemplateBuilderDialog *ui;

    // ── Card list for indicators tab ──
    QVBoxLayout              *m_cardsLayout = nullptr;
    QList<IndicatorRowWidget*> m_indicatorCards;

    // ── Internal parameter list (source of truth for params tab) ──
    QVector<TemplateParam>     m_params;

    // Preserved template id / version / createdAt from edit mode
    QString   m_existingTemplateId;
    QString   m_existingVersion;
    QDateTime m_existingCreatedAt;
};

#endif // STRATEGY_TEMPLATE_BUILDER_DIALOG_H
