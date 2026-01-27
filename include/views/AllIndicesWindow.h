#ifndef ALLINDICESWINDOW_H
#define ALLINDICESWINDOW_H

#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QVector>
#include <QHash>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>

// Forward declaration
class RepositoryManager;

// Data structure for index entry
struct AllIndexEntry {
    QString name;
    int64_t token;
    double ltp = 0.0;
    double change = 0.0;
    double percentChange = 0.0;
    bool selected = false;
    
    AllIndexEntry() = default;
    AllIndexEntry(const QString& n, int64_t t)
        : name(n), token(t) {}
};

// Model for all indices
class AllIndicesModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns {
        COL_SELECTED = 0,
        COL_NAME,
        COL_TOKEN,
        COL_LTP,
        COL_COUNT
    };

    explicit AllIndicesModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_indices.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : COL_COUNT;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void loadFromRepository(RepositoryManager* repoManager);
    void updateIndexPrice(int64_t token, double ltp, double change, double percentChange);
    QStringList getSelectedIndices() const;
    void setSelectedIndices(const QStringList& selectedNames);

private:
    QVector<AllIndexEntry> m_indices;
    QHash<int64_t, int> m_tokenToRow;
};

// Main window showing all indices
class AllIndicesWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AllIndicesWindow(QWidget *parent = nullptr);
    ~AllIndicesWindow();

    void initialize(RepositoryManager* repoManager);
    void setSelectedIndices(const QStringList& selectedNames);
    QStringList getSelectedIndices() const;

signals:
    void selectionChanged(const QStringList& selectedIndices);

private slots:
    void onSearchTextChanged(const QString& text);
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onApplyClicked();

private:
    void setupUI();

    QTableView *m_tableView;
    AllIndicesModel *m_model;
    QSortFilterProxyModel *m_proxyModel;
    QLineEdit *m_searchBox;
    QPushButton *m_selectAllBtn;
    QPushButton *m_deselectAllBtn;
    QPushButton *m_applyBtn;
    
    RepositoryManager* m_repoManager = nullptr;
};

#endif // ALLINDICESWINDOW_H
