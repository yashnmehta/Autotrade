#ifndef INDICATOR_ROW_WIDGET_H
#define INDICATOR_ROW_WIDGET_H

#include "strategy/StrategyTemplate.h"
#include "ui/IndicatorCatalog.h"
#include <QWidget>
#include <QString>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QLabel;
class QFormLayout;
class QDoubleSpinBox;

/**
 * @brief A self-contained form card representing one IndicatorDefinition.
 *
 * Layout (dark card):
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  [Type ▼ auto-populated]  [Symbol ▼]  [TF ▼]  [Price ▼]  [✕]  │
 *   │  ID: [__________]                                               │
 *   │  ── dynamic param rows (from catalog) ─────────────────────── │
 *   │  Fast Period: [12]  Slow Period: [26]  Signal: [9]              │
 *   │  Output: [macd ▼]  (only shown when >1 output)                 │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 * Emits `removeRequested()` when the ✕ button is clicked.
 * Emits `changed()` on any field edit (used to refresh condition context).
 */
class IndicatorRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit IndicatorRowWidget(const QStringList &symbolIds,
                                 int               indexHint = 1,
                                 QWidget          *parent = nullptr);

    // ── Pre-fill from an existing IndicatorDefinition (edit mode) ──
    void populate(const IndicatorDefinition &ind);

    // ── Extract the current values ──
    IndicatorDefinition definition() const;

    // ── Update the symbol list when the Symbols tab changes ──
    void setSymbolIds(const QStringList &ids);

signals:
    void removeRequested();
    void changed();

private slots:
    void onTypeChanged(const QString &type);

private:
    void rebuildParamRows(const IndicatorMeta &meta);
    void clearParamRows();

    // ── Fixed widgets ──
    QComboBox  *m_typeCombo   = nullptr;
    QComboBox  *m_symCombo    = nullptr;
    QComboBox  *m_tfCombo     = nullptr;
    QComboBox  *m_priceCombo  = nullptr;
    QComboBox  *m_outputCombo = nullptr;
    QLabel     *m_outputLabel = nullptr;
    QLineEdit  *m_idEdit      = nullptr;

    // ── Dynamic param widgets (rebuilt when type changes) ──
    // Up to 3 params, each: QLabel + QLineEdit pair inserted into m_paramForm
    QFormLayout       *m_paramForm = nullptr;
    QList<QWidget*>    m_paramWidgets; // all param rows for easy cleanup
    QList<QLineEdit*>  m_paramEdits;   // [param1, param2, param3] — may be fewer

    IndicatorMeta m_currentMeta;
    int           m_indexHint = 1;
};

#endif // INDICATOR_ROW_WIDGET_H
