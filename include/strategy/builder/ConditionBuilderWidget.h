#ifndef CONDITION_BUILDER_WIDGET_H
#define CONDITION_BUILDER_WIDGET_H

#include "strategy/model/ConditionNode.h"
#include "strategy/model/StrategyTemplate.h"
#include <QMap>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QComboBox;

class ConditionBuilderWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConditionBuilderWidget(QWidget *parent = nullptr);

    // ── Context setters ──
    void setSymbolIds(const QStringList &ids);
    void setIndicatorIds(const QStringList &ids);
    void setParamNames(const QStringList &names);

    /**
     * Pass the full indicator definitions so the leaf editor can
     * auto-populate the output series combo per indicator.
     * Key = indicator ID (e.g. "BBANDS_1"), Value = outputs list
     * (e.g. ["upperBand","middleBand","lowerBand"]).
     */
    void setIndicatorOutputMap(const QMap<QString, QStringList> &outputMap);

    // ── Data access ──
    void setCondition(const ConditionNode &root);
    ConditionNode condition() const;

    bool isEmpty() const;
    void clear();

signals:
    void conditionChanged();

private slots:
    void onAddAndGroup();
    void onAddOrGroup();
    void onAddLeaf();
    void onRemoveSelected();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidgetItem *selectedGroupItem() const;
    QTreeWidgetItem *addGroupItem(QTreeWidgetItem *parent,
                                  ConditionNode::NodeType type);
    QTreeWidgetItem *addLeafItem(QTreeWidgetItem *parent,
                                  const ConditionNode &leaf = ConditionNode{});

    ConditionNode nodeFromItem(QTreeWidgetItem *item) const;
    void          itemFromNode(QTreeWidgetItem *parent,
                               const ConditionNode &node);

    void openLeafEditor(QTreeWidgetItem *item);
    QString leafSummary(const ConditionNode &leaf) const;
    QString operandSummary(const Operand &op) const;

    QTreeWidget  *m_tree;
    QPushButton  *m_addAndBtn;
    QPushButton  *m_addOrBtn;
    QPushButton  *m_addLeafBtn;
    QPushButton  *m_removeBtn;

    QStringList               m_symbolIds;
    QStringList               m_indicatorIds;
    QStringList               m_paramNames;
    QMap<QString,QStringList> m_indicatorOutputMap;  // id → outputs[]
};

#endif // CONDITION_BUILDER_WIDGET_H
