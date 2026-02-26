#ifndef INDICATOR_PICKER_DIALOG_H
#define INDICATOR_PICKER_DIALOG_H

#include "ui/IndicatorCatalog.h"
#include <QDialog>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QLineEdit;

/**
 * @brief Modal picker that lets the user browse all TA-Lib indicators by
 *        group, with live search filter and a description panel.
 *
 * Usage:
 *   IndicatorPickerDialog dlg(symbolIds, this);
 *   if (dlg.exec() == QDialog::Accepted) {
 *       IndicatorMeta meta = dlg.selectedMeta();
 *       QString       symId = dlg.selectedSymbolId();
 *       QString       indId = dlg.suggestedId(); // e.g. "RSI_2" (auto-suffixed)
 *   }
 */
class IndicatorPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit IndicatorPickerDialog(const QStringList &symbolIds,
                                   int               existingCount = 0,
                                   QWidget          *parent = nullptr);

    IndicatorMeta selectedMeta()     const { return m_selected;   }
    QString       selectedSymbolId() const { return m_symbolId;   }
    QString       suggestedId()      const { return m_suggestedId;}
    /// The output series the user selected (e.g. "upperBand", "macd", "signal")
    /// Empty for single-output indicators.
    QString       selectedOutput()   const { return m_outputSel;  }
    /// The candle timeframe the user selected (e.g. "5", "15", "D")
    QString       selectedTimeframe() const { return m_timeframe; }

    /// Pre-set the search filter (e.g. to current indicator type in edit mode)
    void setInitialFilter(const QString &text);

protected:
    void accept() override;

private slots:
    void onFilterChanged(const QString &text);
    void onItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    void buildTree(const QString &filter = {});
    void updateDescription(const IndicatorMeta &m);

    QLineEdit    *m_filterEdit  = nullptr;
    QTreeWidget  *m_tree        = nullptr;
    QLabel       *m_descLabel   = nullptr;
    QLabel       *m_paramLabel  = nullptr;
    class QComboBox *m_symCombo    = nullptr;
    class QComboBox *m_outputCombo = nullptr;  // output series selector
    class QComboBox *m_tfCombo     = nullptr;  // timeframe selector

    IndicatorMeta m_selected;
    QString       m_symbolId;
    QString       m_suggestedId;
    QString       m_outputSel;
    QString       m_timeframe;
    int           m_existingCount = 0;
    QStringList   m_symbolIds;
};

#endif // INDICATOR_PICKER_DIALOG_H
