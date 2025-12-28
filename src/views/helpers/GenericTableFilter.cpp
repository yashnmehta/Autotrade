#include "views/helpers/GenericTableFilter.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QSet>
#include <QDebug>

GenericTableFilter::GenericTableFilter(int column, QAbstractItemModel* model, QTableView* tableView, QWidget* parent)
    : QWidget(parent)
    , m_column(column)
    , m_model(model)
    , m_tableView(tableView)
    , m_filterRowOffset(1) // Default assumes this widget is IN the filter row at index 0
    , m_summaryRowOffset(0)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    
    m_filterButton = new QPushButton("▼ Filter");
    m_filterButton->setStyleSheet(
        "QPushButton { "
        "background: #ffffff; "
        "color: #333333; "
        "border: 1px solid #cccccc; "
        "border-radius: 2px; "
        "font-size: 10px; "
        "padding: 2px; "
        "} "
        "QPushButton:hover { "
        "background: #f0f0f0; "
        "}"
    );
    
    layout->addWidget(m_filterButton);
    connect(m_filterButton, &QPushButton::clicked, this, &GenericTableFilter::showFilterPopup);
}

void GenericTableFilter::clear()
{
    m_selectedValues.clear();
    updateButtonDisplay();
}

void GenericTableFilter::updateButtonDisplay()
{
    if (m_selectedValues.isEmpty()) {
        m_filterButton->setText("▼ Filter");
    } else {
        m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
    }
}

void GenericTableFilter::showFilterPopup()
{
    if (!m_model) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Filter Column");
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QListWidget* listWidget = new QListWidget(&dialog);
    
    // Get unique values
    QStringList uniqueValues = getUniqueValuesForColumn();
    for (const QString& val : uniqueValues) {
        QListWidgetItem* item = new QListWidgetItem(val, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_selectedValues.contains(val) ? Qt::Checked : Qt::Unchecked);
    }
    
    layout->addWidget(listWidget);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        m_selectedValues.clear();
        for (int i = 0; i < listWidget->count(); ++i) {
            if (listWidget->item(i)->checkState() == Qt::Checked) {
                m_selectedValues.append(listWidget->item(i)->text());
            }
        }
        updateButtonDisplay();
        emit filterChanged(m_column, m_selectedValues);
    }
}

QStringList GenericTableFilter::getUniqueValuesForColumn() const
{
    QSet<QString> values;
    if (!m_model) return QStringList();
    
    int rowCount = m_model->rowCount();
    int endRow = rowCount - m_summaryRowOffset;
    
    for (int i = m_filterRowOffset; i < endRow; ++i) {
        QString val = m_model->data(m_model->index(i, m_column), Qt::DisplayRole).toString();
        if (!val.isEmpty()) {
            values.insert(val);
        }
    }
    
    QStringList sorted = values.values();
    sorted.sort();
    return sorted;
}
