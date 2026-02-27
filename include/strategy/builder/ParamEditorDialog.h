#ifndef PARAM_EDITOR_DIALOG_H
#define PARAM_EDITOR_DIALOG_H

/**
 * @file ParamEditorDialog.h
 * @brief Form-based dialog for adding or editing a single TemplateParam.
 *
 * Replaces the cramped table-cell editing in the Parameters tab.
 * Opens as a modal popup from the StrategyTemplateBuilderDialog.
 *
 * The form has two visual modes:
 *   FIXED (Int/Double/Bool/String):
 *     ┌──────────────────────────────────────────────┐
 *     │  Name:        [PARAM_1        ]              │
 *     │  Label:       [My Parameter   ]              │
 *     │  Type:        [Double ▾]                     │
 *     │  Default:     [0.0            ]              │
 *     │  Min:         [         ] Max: [         ]   │
 *     │  Description: [                          ]   │
 *     │  ☐ Locked (cannot change at deploy time)     │
 *     └──────────────────────────────────────────────┘
 *
 *   EXPRESSION (formula + trigger):
 *     ┌──────────────────────────────────────────────┐
 *     │  Name:        [SL_LEVEL       ]              │
 *     │  Label:       [Stop Loss Level]              │
 *     │  Type:        [Expression ▾]                 │
 *     │  ┌─ Formula ────────────────────────────── ┐ │
 *     │  │ [ATR(REF_1, 14) * 2.5                 ] │ │
 *     │  │ Formula help: LTP() ATR() RSI() IV()... │ │
 *     │  └─────────────────────────────────────── ┘ │
 *     │  ┌─ Recalculation Trigger ───────────────┐  │
 *     │  │ Trigger:   [On Candle Close ▾]        │  │
 *     │  │ Timeframe: [5m ▾]                     │  │
 *     │  │ Interval:  [300 sec] (if OnSchedule)  │  │
 *     │  └───────────────────────────────────────┘  │
 *     │  Description: [                          ]  │
 *     │  ☐ Locked                                   │
 *     └──────────────────────────────────────────────┘
 */

#include "strategy/model/StrategyTemplate.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QStackedWidget;

class ParamEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit ParamEditorDialog(QWidget *parent = nullptr);
    ~ParamEditorDialog() override = default;

    /// Set the param to edit (for edit mode). Populates all fields.
    void setParam(const TemplateParam &p);

    /// Returns the param built from the current form state.
    TemplateParam param() const;

    /// Set the window title for add vs edit mode
    void setEditMode(bool editing);

    /// Pass context from the template builder so the formula palette
    /// can show available symbol slots, parameters, and indicators.
    void setContext(const QStringList &symbolIds,
                    const QStringList &paramNames,
                    const QStringList &indicatorIds);

protected:
    void accept() override;

private slots:
    void onTypeChanged(int index);
    void onTriggerChanged(int index);
    void insertTextAtCursor(const QString &text);

private:
    void buildUi();
    void rebuildPalette();
    bool validate();
    QString typeToString(ParamValueType t) const;

    // ── Identity ──
    QLineEdit   *m_nameEdit        = nullptr;
    QLineEdit   *m_labelEdit       = nullptr;
    QComboBox   *m_typeCombo       = nullptr;

    // ── Fixed value section ──
    QGroupBox   *m_fixedGroup      = nullptr;
    QLineEdit   *m_defaultEdit     = nullptr;
    QLineEdit   *m_minEdit         = nullptr;
    QLineEdit   *m_maxEdit         = nullptr;

    // ── Expression section ──
    QGroupBox   *m_exprGroup       = nullptr;
    QPlainTextEdit *m_formulaEdit  = nullptr;
    QLabel      *m_formulaHelpLabel = nullptr;

    // ── Reference palette (clickable insert chips) ──
    QGroupBox   *m_paletteGroup    = nullptr;
    QWidget     *m_paletteContent  = nullptr;

    // ── Trigger section (inside expression group) ──
    QGroupBox   *m_triggerGroup    = nullptr;
    QComboBox   *m_triggerCombo    = nullptr;
    QComboBox   *m_timeframeCombo  = nullptr;
    QLabel      *m_timeframeLabel  = nullptr;
    QSpinBox    *m_intervalSpin    = nullptr;
    QLabel      *m_intervalLabel   = nullptr;

    // ── Common ──
    QPlainTextEdit *m_descEdit     = nullptr;
    QCheckBox   *m_lockedCheck     = nullptr;

    // ── Context for palette ──
    QStringList  m_symbolIds;
    QStringList  m_paramNames;
    QStringList  m_indicatorIds;
};

#endif // PARAM_EDITOR_DIALOG_H
