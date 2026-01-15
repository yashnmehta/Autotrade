#include "views/IndicesView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDebug>

IndicesView::IndicesView(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    
    // detailed dummy data for testing UI
    // detailed dummy data for testing UI
    updateIndex(26000, "NIFTY 50", 22500.00, 111.94, 0.50);
    updateIndex(26001, "BANKNIFTY", 48000.00, -120.30, -0.25);
}

IndicesView::~IndicesView()
{
}

void IndicesView::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({"Index", "LTP", "Chg", "%"});
    
    // Hide vertical header
    m_table->verticalHeader()->setVisible(false);
    
    // Style
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Column sizing
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    header->setSectionResizeMode(COL_LTP, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_CHANGE, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_PERCENT, QHeaderView::ResizeToContents);
    header->setVisible(false); // Hide header as per reference
    
    // Header styling (even if hidden, good to have)
    m_table->setStyleSheet(
        "QTableWidget { background-color: #ffffff; color: #000000; border: none; font-weight: bold; font-family: 'Segoe UI', sans-serif; }"
        "QTableWidget::item { padding: 4px; border-bottom: 1px solid #eeeeee; }"
        "QTableWidget::item:selected { background-color: #e5f3ff; color: #000000; }"
    );

    layout->addWidget(m_table);
}

void IndicesView::updateIndex(int64_t token, const QString& symbol, double ltp, double change, double percentChange)
{
    int row = getRowForToken(token);
    
    // Name
    QTableWidgetItem *nameItem = m_table->item(row, COL_NAME);
    if (!nameItem) {
        nameItem = new QTableWidgetItem(symbol);
        nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        // Make name bold/black
        nameItem->setForeground(QBrush(QColor("#000000")));
        m_table->setItem(row, COL_NAME, nameItem);
    }
    
    // Calculate Change
    // double prev = ltp / (1.0 + (percentChange / 100.0));
    double absChg = change; // Use passed change value
    
    // Color coding based on change
    QColor textColor = (percentChange >= 0) ? QColor("#00aa00") : QColor("#ff0000"); // Green : Red
    
    // LTP Column: "LTP (Change)" format or just LTP?
    // Reference shows "32214.45 (-168.85)" in one line with arrow.
    // We have separate columns. Let's format LTP to include change if we want exact match, 
    // OR keep strict columns but color them.
    // Let's try to match reference: Name | Value (Change) | Arrow
    // But our columns are set up as 4. Let's use them effectively.
    
    // LTP
    QTableWidgetItem *ltpItem = m_table->item(row, COL_LTP);
    if (!ltpItem) {
        ltpItem = new QTableWidgetItem();
        ltpItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, COL_LTP, ltpItem);
    }
    
    // Format: "24701.00 (-221.40)"
    QString chgPrefix = (absChg > 0) ? "+" : "";
    QString text = QString("%1 (%2%3)")
                       .arg(ltp, 0, 'f', 2)
                       .arg(chgPrefix)
                       .arg(absChg, 0, 'f', 2);
    
    ltpItem->setText(text);
    ltpItem->setForeground(textColor);
    
    // Percent (hidden or separate?) Reference doesn't clearly show %. Use arrow column instead of percent column?
    // Let's keep percent column but formatted nicely.
    QTableWidgetItem *pctItem = m_table->item(row, COL_PERCENT);
    if (!pctItem) {
        pctItem = new QTableWidgetItem();
        pctItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        m_table->setItem(row, COL_PERCENT, pctItem);
    }
    
    // Use arrow character
    QString arrow = (percentChange >= 0) ? "▲" : "▼";
    pctItem->setText(arrow);
    pctItem->setForeground(textColor);
    
    // Clear unused columns if we merging
    // Actually, let's just validte data.
    
}

int IndicesView::getRowForToken(int64_t token)
{
    if (m_tokenToRow.contains(token)) {
        return m_tokenToRow[token];
    }
    
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_tokenToRow.insert(token, row);
    return row;
}

void IndicesView::onIndexReceived(const UDP::IndexTick& tick)
{
    // Log Stage 4: UI Reception
    qDebug() << "[IndicesView] Received tick for:" << tick.name << "Token:" << tick.token << "Value:" << tick.value;

    // Filter/Map Logic
    // Needs optimization for real usage (e.g. hash map of tracked tokens)
    
    // NSECM Nifty 50 (Token usually 0 or specific depending on feed, Name "Nifty 50")
    // BSE Sensex (Token 1, Name "SENSEX")
    
    QString name = QString::fromLatin1(tick.name).trimmed();
    
    // For BSE, name might be empty, so map from token
    if (tick.exchangeSegment == UDP::ExchangeSegment::BSECM || tick.exchangeSegment == UDP::ExchangeSegment::BSEFO) {
        if (tick.token == 1) name = "SENSEX";
        else if (tick.token == 2) name = "BSE 100"; // Example
        else return; // Ignore other BSE indices for now
    }
    
    // For NSE, name comes from broadcast but we might want to standardize
    if (name.compare("Nifty 50", Qt::CaseInsensitive) == 0 || name == "NIFTY") {
        name = "NIFTY 50";
    } else if (name.compare("Nifty Bank", Qt::CaseInsensitive) == 0 || name == "BANKNIFTY") {
        name = "BANKNIFTY";
    }
    
    // Only update if it's one of the watched indices
    if (m_tokenToRow.contains(tick.token)) { // Using m_tokenToRow for consistency with existing updateIndex
        updateIndex(tick.token, name, tick.value, tick.change, tick.changePercent);
    } else {
        // Or add it if we want dynamic list
        if (name == "NIFTY 50" || name == "BANKNIFTY" || name == "SENSEX") {
            updateIndex(tick.token, name, tick.value, tick.change, tick.changePercent);
        }
    }
}

void IndicesView::clear()
{
    m_table->setRowCount(0); // Changed from m_tableWidget to m_table
    m_tokenToRow.clear(); // Changed from m_rowMap to m_tokenToRow
}
