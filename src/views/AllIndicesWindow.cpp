#include "views/AllIndicesWindow.h"
#include "ui_AllIndicesWindow.h"
#include "repository/RepositoryManager.h"
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QDebug>
#include <QSettings>

// ========== AllIndicesModel Implementation ==========

QVariant AllIndicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_indices.size())
        return QVariant();

    const AllIndexEntry& entry = m_indices[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case COL_NAME:
                return entry.name;
            case COL_TOKEN:
                return QString::number(entry.token);
            case COL_LTP:
                if (entry.ltp > 0.0) {
                    return QString("%1 (%2%3)")
                        .arg(entry.ltp, 0, 'f', 2)
                        .arg(entry.change >= 0 ? "+" : "")
                        .arg(entry.change, 0, 'f', 2);
                }
                return QString("-");
        }
    }
    else if (role == Qt::CheckStateRole && index.column() == COL_SELECTED) {
        return entry.selected ? Qt::Checked : Qt::Unchecked;
    }
    else if (role == Qt::ForegroundRole && index.column() == COL_LTP) {
        if (entry.ltp > 0.0) {
            return (entry.percentChange >= 0) ? QColor("#00aa00") : QColor("#ff0000");
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case COL_SELECTED:
                return int(Qt::AlignCenter | Qt::AlignVCenter);
            case COL_NAME:
                return int(Qt::AlignLeft | Qt::AlignVCenter);
            case COL_TOKEN:
            case COL_LTP:
                return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return QVariant();
}

QVariant AllIndicesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case COL_SELECTED: return "✓";
            case COL_NAME: return "Index Name";
            case COL_TOKEN: return "Token";
            case COL_LTP: return "LTP";
        }
    }
    return QVariant();
}

Qt::ItemFlags AllIndicesModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (index.column() == COL_SELECTED) {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

bool AllIndicesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_indices.size())
        return false;

    if (role == Qt::CheckStateRole && index.column() == COL_SELECTED) {
        m_indices[index.row()].selected = (value.toInt() == Qt::Checked);
        emit dataChanged(index, index);
        return true;
    }

    return false;
}

void AllIndicesModel::loadFromRepository(RepositoryManager* repoManager)
{
    if (!repoManager) return;

    beginResetModel();
    m_indices.clear();
    m_tokenToRow.clear();

    const auto& indexMap = repoManager->getIndexNameTokenMap();
    
    qDebug() << "[AllIndicesModel] Loading" << indexMap.size() << "indices";

    // Convert to vector and sort by name
    QVector<QPair<QString, int64_t>> sortedIndices;
    for (auto it = indexMap.constBegin(); it != indexMap.constEnd(); ++it) {
        sortedIndices.append(qMakePair(it.key(), it.value()));
    }
    
    std::sort(sortedIndices.begin(), sortedIndices.end(),
              [](const QPair<QString, int64_t>& a, const QPair<QString, int64_t>& b) {
                  return a.first < b.first;
              });

    // Add to model
    for (const auto& pair : sortedIndices) {
        int row = m_indices.size();
        m_indices.append(AllIndexEntry(pair.first, pair.second));
        m_tokenToRow[pair.second] = row;
    }

    endResetModel();
    
    qDebug() << "[AllIndicesModel] Loaded" << m_indices.size() << "indices";
}

void AllIndicesModel::updateIndexPrice(int64_t token, double ltp, double change, double percentChange)
{
    if (!m_tokenToRow.contains(token))
        return;

    int row = m_tokenToRow[token];
    m_indices[row].ltp = ltp;
    m_indices[row].change = change;
    m_indices[row].percentChange = percentChange;

    emit dataChanged(index(row, COL_LTP), index(row, COL_LTP));
}

QStringList AllIndicesModel::getSelectedIndices() const
{
    QStringList selected;
    for (const AllIndexEntry& entry : m_indices) {
        if (entry.selected) {
            selected.append(entry.name);
        }
    }
    return selected;
}

void AllIndicesModel::setSelectedIndices(const QStringList& selectedNames)
{
    beginResetModel();
    for (AllIndexEntry& entry : m_indices) {
        entry.selected = selectedNames.contains(entry.name);
    }
    endResetModel();
}

// ========== AllIndicesWindow Implementation ==========

AllIndicesWindow::AllIndicesWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AllIndicesWindow)
    , m_model(new AllIndicesModel(this))
    , m_proxyModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    // Setup proxy model for filtering
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(AllIndicesModel::COL_NAME);

    // Table view configuration
    ui->tableView->setModel(m_proxyModel);
    ui->tableView->verticalHeader()->setVisible(false);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // Column widths
    ui->tableView->setColumnWidth(AllIndicesModel::COL_SELECTED, 40);
    ui->tableView->setColumnWidth(AllIndicesModel::COL_NAME, 300);
    ui->tableView->setColumnWidth(AllIndicesModel::COL_TOKEN, 80);

    // Connections
    connect(ui->searchBox, &QLineEdit::textChanged, this, &AllIndicesWindow::onSearchTextChanged);
    connect(ui->selectAllBtn, &QPushButton::clicked, this, &AllIndicesWindow::onSelectAllClicked);
    connect(ui->deselectAllBtn, &QPushButton::clicked, this, &AllIndicesWindow::onDeselectAllClicked);
    connect(ui->applyBtn, &QPushButton::clicked, this, &AllIndicesWindow::onApplyClicked);

    setWindowFlags(Qt::Window);
}

AllIndicesWindow::~AllIndicesWindow()
{
    delete ui;
}

void AllIndicesWindow::initialize(RepositoryManager* repoManager)
{
    m_repoManager = repoManager;
    m_model->loadFromRepository(repoManager);
}

void AllIndicesWindow::setSelectedIndices(const QStringList& selectedNames)
{
    m_model->setSelectedIndices(selectedNames);
}

QStringList AllIndicesWindow::getSelectedIndices() const
{
    return m_model->getSelectedIndices();
}

void AllIndicesWindow::onSearchTextChanged(const QString& text)
{
    m_proxyModel->setFilterWildcard(text);
}

void AllIndicesWindow::onSelectAllClicked()
{
    // Select all visible (filtered) rows
    for (int i = 0; i < m_proxyModel->rowCount(); ++i) {
        QModelIndex proxyIndex = m_proxyModel->index(i, AllIndicesModel::COL_SELECTED);
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        m_model->setData(sourceIndex, Qt::Checked, Qt::CheckStateRole);
    }
}

void AllIndicesWindow::onDeselectAllClicked()
{
    // Deselect all visible (filtered) rows
    for (int i = 0; i < m_proxyModel->rowCount(); ++i) {
        QModelIndex proxyIndex = m_proxyModel->index(i, AllIndicesModel::COL_SELECTED);
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        m_model->setData(sourceIndex, Qt::Unchecked, Qt::CheckStateRole);
    }
}

void AllIndicesWindow::onApplyClicked()
{
    QStringList selected = m_model->getSelectedIndices();
    qDebug() << "[AllIndicesWindow] Applying selection:" << selected.size() << "indices";
    
    // Save to settings
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.setValue("indices/selected", selected);
    
    emit selectionChanged(selected);
    
    // Close window
    hide();
}
