#ifndef INDICESVIEW_H
#define INDICESVIEW_H

#include <QWidget>
#include <QTableWidget>
#include <QVector>
#include "api/XTSTypes.h"
#include <QHeaderView>
#include "udp/UDPTypes.h"

// Forward declaration
namespace UDP { struct IndexTick; }

class IndicesView : public QWidget
{
    Q_OBJECT

public:
    explicit IndicesView(QWidget *parent = nullptr);
    ~IndicesView();

    // Call this to update an index value
    // name: Index Name (e.g., "NIFTY 50")
    // ltp: Last Traded Price
    // change: Absolute change
    // percentChange: Percentage change
    void updateIndex(int64_t token, const QString &name, double ltp, double change, double percentChange);
    
    // Process broadcast updates
    void onIndexReceived(const UDP::IndexTick& tick);

    // Clear all data
    void clear();

signals:
    void hideRequested();

private:
    void setupUI();
    int getRowForToken(int64_t token);

    QTableWidget *m_table;
    QMap<int64_t, int> m_tokenToRow; // Map token to row index
    
    // Columns
    enum Columns {
        COL_NAME = 0,
        COL_LTP,
        COL_CHANGE, // Absolute change (calculated if prev close known, or just passed)
        COL_PERCENT,
        COL_COUNT
    };
};

#endif // INDICESVIEW_H
