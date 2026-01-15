#ifndef INDICESVIEW_H
#define INDICESVIEW_H

#include <QWidget>
#include <QTableWidget>
#include <QVector>
#include "api/XTSTypes.h"

class IndicesView : public QWidget
{
    Q_OBJECT

public:
    explicit IndicesView(QWidget *parent = nullptr);
    ~IndicesView();

    // Update index data
    void updateIndex(int64_t token, const QString& symbol, double ltp, double percentChange);
    
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
