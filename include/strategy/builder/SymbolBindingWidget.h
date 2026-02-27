#ifndef SYMBOL_BINDING_WIDGET_H
#define SYMBOL_BINDING_WIDGET_H

#include "repository/ContractData.h"
#include "strategy/model/StrategyTemplate.h"
#include <QWidget>
#include <QVector>

class QLineEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

/**
 * @brief Widget for resolving a single SymbolDefinition slot to a concrete ContractData.
 */
class SymbolBindingWidget : public QWidget {
    Q_OBJECT
public:
    explicit SymbolBindingWidget(const SymbolDefinition &def, QWidget *parent = nullptr);

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

#endif // SYMBOL_BINDING_WIDGET_H
