#ifndef STRATEGY_TABLE_MODEL_H
#define STRATEGY_TABLE_MODEL_H

#include "models/StrategyInstance.h"
#include <QAbstractTableModel>
#include <QVector>


class StrategyTableModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum Column {
    COL_INSTANCE_NAME = 0,
    COL_STATUS,
    COL_MTM,
    COL_STOP_LOSS,
    COL_TARGET,
    COL_ENTRY_PRICE,
    COL_QUANTITY,
    COL_POSITIONS,
    COL_ORDERS,
    COL_DURATION,
    COL_SYMBOL,
    COL_STRATEGY_TYPE,
    COL_CREATED_AT,
    COL_LAST_UPDATED,
    COL_COUNT
  };

  explicit StrategyTableModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;

  void setInstances(const QVector<StrategyInstance> &instances);
  void addInstance(const StrategyInstance &instance);
  void updateInstance(const StrategyInstance &instance);
  void removeInstance(qint64 instanceId);

  StrategyInstance instanceAt(int row) const;
  const QVector<StrategyInstance> &allInstances() const { return m_instances; }
  int findRowById(qint64 instanceId) const;

signals:
  void instanceEdited(const StrategyInstance &instance);

private:
  QVector<StrategyInstance> m_instances;

  QVariant displayForColumn(const StrategyInstance &instance,
                            Column column) const;
  QVariant sortValueForColumn(const StrategyInstance &instance,
                              Column column) const;
  QString formatDuration(const StrategyInstance &instance) const;
};

#endif // STRATEGY_TABLE_MODEL_H
