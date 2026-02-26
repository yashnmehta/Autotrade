#include "views/helpers/GenericTableFilter.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QSet>
#include <QDebug>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QFrame>

GenericTableFilter::GenericTableFilter(int column, QAbstractItemModel* model, QTableView* tableView, 
                                       FilterMode mode, QWidget* parent)
    : QWidget(parent)
    , m_column(column)
    , m_model(model)
    , m_tableView(tableView)
    , m_filterMode(mode)
    , m_filterButton(nullptr)
    , m_filterLineEdit(nullptr)
    , m_filterRowOffset(1) // Default assumes this widget is IN the filter row at index 0
    , m_summaryRowOffset(0)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(1);
    
    // Common styles
    QString lineEditStyle = 
        "QLineEdit { "
        "background: #ffffff; "
        "color: #1e293b; "
        "border: 1px solid #e2e8f0; "
        "border-radius: 2px; "
        "font-size: 10px; "
        "padding: 2px; "
        "} "
        "QLineEdit:focus { "
        "border: 1px solid #3b82f6; "
        "}";
    
    QString buttonStyle = 
        "QPushButton { "
        "background: #ffffff; "
        "color: #1e293b; "
        "border: 1px solid #e2e8f0; "
        "border-radius: 2px; "
        "font-size: 10px; "
        "padding: 2px; "
        "min-width: 16px; "
        "max-width: 20px; "
        "} "
        "QPushButton:hover { "
        "background: #f1f5f9; "
        "}";
    
    if (m_filterMode == InlineMode) {
        // Inline text search mode only
        m_filterLineEdit = new QLineEdit();
        m_filterLineEdit->setPlaceholderText("Filter...");
        m_filterLineEdit->setStyleSheet(lineEditStyle);
        layout->addWidget(m_filterLineEdit);
        connect(m_filterLineEdit, &QLineEdit::textChanged, this, &GenericTableFilter::onTextChanged);
        
    } else if (m_filterMode == CombinedMode) {
        // Combined mode: Text input + dropdown button (Excel-style)
        m_filterLineEdit = new QLineEdit();
        m_filterLineEdit->setPlaceholderText("Filter...");
        m_filterLineEdit->setStyleSheet(lineEditStyle);
        layout->addWidget(m_filterLineEdit, 1);  // Stretch factor 1
        connect(m_filterLineEdit, &QLineEdit::textChanged, this, &GenericTableFilter::onTextChanged);
        
        m_filterButton = new QPushButton("▼");
        m_filterButton->setToolTip("Multi-select filter");
        m_filterButton->setStyleSheet(buttonStyle);
        layout->addWidget(m_filterButton);
        connect(m_filterButton, &QPushButton::clicked, this, &GenericTableFilter::showFilterPopup);
        
    } else {
        // Popup checkbox mode (PopupMode - default)
        m_filterButton = new QPushButton("▼ Filter");
        m_filterButton->setStyleSheet(
            "QPushButton { "
            "background: #ffffff; "
            "color: #1e293b; "
            "border: 1px solid #e2e8f0; "
            "border-radius: 2px; "
            "font-size: 10px; "
            "padding: 2px; "
            "} "
            "QPushButton:hover { "
            "background: #f1f5f9; "
            "}"
        );
        layout->addWidget(m_filterButton);
        connect(m_filterButton, &QPushButton::clicked, this, &GenericTableFilter::showFilterPopup);
    }
}


void GenericTableFilter::clear()
{
    m_selectedValues.clear();
    if (m_filterLineEdit) {
        m_filterLineEdit->clear();
    }
    updateButtonDisplay();
}

void GenericTableFilter::updateButtonDisplay()
{
    if (m_filterButton) {
        if (m_selectedValues.isEmpty()) {
            m_filterButton->setText("▼ Filter");
        } else {
            m_filterButton->setText(QString("▼ (%1)").arg(m_selectedValues.count()));
        }
    }
}

QString GenericTableFilter::filterText() const
{
    return m_filterLineEdit ? m_filterLineEdit->text() : QString();
}

void GenericTableFilter::onTextChanged(const QString& text)
{
    qDebug() << "[GenericTableFilter] onTextChanged: col=" << m_column << "text=" << text;
    emit textFilterChanged(m_column, text);
}

void GenericTableFilter::showFilterPopup()
{
    if (!m_model) return;

    QDialog dialog(this);
    dialog.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    dialog.setWindowTitle("Filter");
    
    // Position the dialog below the button
    QPoint globalPos = m_filterButton->mapToGlobal(QPoint(0, m_filterButton->height()));
    dialog.move(globalPos);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    
    // 1. Sort Options
    QHBoxLayout* sortLayout = new QHBoxLayout();
    QPushButton* sortAscBtn = new QPushButton("Sort A to Z");
    QPushButton* sortDescBtn = new QPushButton("Sort Z to A");
    
    // Style the sort buttons to look like menu items
    QString sortBtnStyle = "QPushButton { text-align: left; border: none; padding: 4px; } QPushButton:hover { background-color: #f1f5f9; }";
    sortAscBtn->setStyleSheet(sortBtnStyle);
    sortDescBtn->setStyleSheet(sortBtnStyle);
    
    sortLayout->addWidget(sortAscBtn);
    sortLayout->addWidget(sortDescBtn);
    
    QFrame* line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    
    layout->addWidget(sortAscBtn);
    layout->addWidget(sortDescBtn);
    layout->addWidget(line1);

    // 2. Search Box
    QLineEdit* searchBox = new QLineEdit();
    searchBox->setPlaceholderText("Search...");
    layout->addWidget(searchBox);
    
    // 3. Select All Checkbox
    QCheckBox* selectAllBox = new QCheckBox("(Select All)");
    layout->addWidget(selectAllBox);
    
    // 4. List Widget
    QListWidget* listWidget = new QListWidget();
    listWidget->setMinimumHeight(150);
    listWidget->setMinimumWidth(200);
    
    // Get unique values to populate
    QStringList uniqueValues = getUniqueValuesForColumn();
    // Cache items for filtering
    QList<QListWidgetItem*> allItems;

    for (const QString& val : uniqueValues) {
        QListWidgetItem* item = new QListWidgetItem(val, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        // Check if currently selected or if no selection active (implied all)
        bool isChecked = m_selectedValues.isEmpty() || m_selectedValues.contains(val);
        item->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
        allItems.append(item);
    }
    
    layout->addWidget(listWidget);
    
    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    
    // Logic: Connect Search Box
    connect(searchBox, &QLineEdit::textChanged, [&, listWidget](const QString& text) {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem* item = listWidget->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });
    
    // Logic: Connect Select All
    connect(selectAllBox, &QCheckBox::stateChanged, [&, listWidget](int state) {
        Qt::CheckState checkState = (state == Qt::Checked) ? Qt::Checked : Qt::Unchecked;
        
        bool isSearching = !searchBox->text().isEmpty();
        
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem* item = listWidget->item(i);
            
            if (checkState == Qt::Unchecked) {
                // If unchecking "Select All", uncheck EVERYTHING (Hidden or Visible)
                // This allows the user to "Clear All" easily before searching.
                item->setCheckState(Qt::Unchecked);
            } else {
                // If checking "Select All", only check VISIBLE items.
                // This supports the "Search -> Select All Results" workflow.
                if (!item->isHidden()) {
                    item->setCheckState(Qt::Checked);
                }
            }
        }
    });

    // Check "Select All" initially if all items are checked
    bool allChecked = true;
    for (auto* item : allItems) {
        if (item->checkState() == Qt::Unchecked) {
            allChecked = false;
            break;
        }
    }
    selectAllBox->setChecked(allChecked);

    // Logic: Sort Buttons
    connect(sortAscBtn, &QPushButton::clicked, [&, this]() {
        emit sortRequested(m_column, Qt::AscendingOrder);
        dialog.accept();
    });
    
    connect(sortDescBtn, &QPushButton::clicked, [&, this]() {
        emit sortRequested(m_column, Qt::DescendingOrder);
        dialog.accept();
    });
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    // Show
    if (dialog.exec() == QDialog::Accepted) {
        if (sortAscBtn->hasFocus() || sortDescBtn->hasFocus()) return; // Already handled

        m_selectedValues.clear();
        bool allSelected = true;
        
        for (int i = 0; i < listWidget->count(); ++i) {
            if (listWidget->item(i)->checkState() == Qt::Checked) {
                m_selectedValues.append(listWidget->item(i)->text());
            } else {
                allSelected = false;
            }
        }
        
        // If all selected, clear the list to indicate "no filter" (show all)
        // This is important for performance and logic
        if (uniqueValues.count() == m_selectedValues.count()) {
             m_selectedValues.clear();
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
