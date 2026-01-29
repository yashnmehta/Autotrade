#ifndef INDICESVIEW_H
#define INDICESVIEW_H

#include "api/XTSTypes.h"
#include "udp/UDPTypes.h"
#include <QAbstractTableModel>
#include <QHash>
#include <QTableView>
#include <QTimer>
#include <QVector>
#include <QWidget>

// Forward declarations
namespace UDP {
struct IndexTick;
}
class RepositoryManager;

// Data structure for index
struct IndexData {
  QString name;
  double ltp = 0.0;
  double change = 0.0;
  double percentChange = 0.0;

  IndexData() = default;
  IndexData(const QString &n, double l, double c, double pc)
      : name(n), ltp(l), change(c), percentChange(pc) {}
};

// ✅ PERFORMANCE: Custom model - no widget item overhead!
class IndicesModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum Columns { COL_NAME = 0, COL_LTP, COL_CHANGE, COL_PERCENT, COL_COUNT };

  explicit IndicesModel(QObject *parent = nullptr)
      : QAbstractTableModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : m_indices.size();
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : COL_COUNT;
  }

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  // Update or add index data
  void updateIndex(const QString &name, double ltp, double change,
                   double percentChange);
  void clear();

private:
  QVector<IndexData> m_indices;
  QHash<QString, int> m_nameToRow;
};

// ✅ PERFORMANCE: QTableView instead of QTableWidget
class IndicesView : public QWidget {
  Q_OBJECT

public:
  explicit IndicesView(QWidget *parent = nullptr);
  ~IndicesView();

  void updateIndex(const QString &name, double ltp, double change,
                   double percentChange);
  void clear();
  void initialize(RepositoryManager *repoManager);
  void reloadSelectedIndices(const QStringList &selectedIndices);

  // Context menu support
  void removeIndex(const QString &name);

public slots:
  void showContextMenu(const QPoint &pos);

signals:
  void hideRequested();

public slots:
  void onIndexReceived(const UDP::IndexTick &tick);

private slots:
  void processPendingUpdates();

private:
  void setupUI();

  QTableView *m_view;    // ✅ View (no item overhead)
  IndicesModel *m_model; // ✅ Model (efficient data storage)
  QHash<QString, IndexData> m_pendingUpdates;
  QTimer *m_updateTimer;
  RepositoryManager *m_repoManager = nullptr;
  QStringList m_selectedIndices;
};

#endif // INDICESVIEW_H
