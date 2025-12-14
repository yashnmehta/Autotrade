#ifndef MARKETWATCHMODEL_H
#define MARKETWATCHMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QString>

/**
 * @brief Data structure for a single scrip (security) in the market watch
 * 
 * Supports both regular scrips and blank separator rows for organization.
 */
struct ScripData
{
    // Identity
    QString symbol;          // e.g., "NIFTY 50", "RELIANCE"
    QString exchange;        // e.g., "NSE", "BSE", "NFO"
    int token = 0;           // Unique token ID for API subscriptions
    bool isBlankRow = false; // True for visual separator rows
    
    // Price data
    double ltp = 0.0;              // Last Traded Price
    double change = 0.0;           // Absolute change
    double changePercent = 0.0;    // Percentage change
    qint64 volume = 0;             // Total volume
    double bid = 0.0;              // Best bid price
    double ask = 0.0;              // Best ask price
    double high = 0.0;             // Day high
    double low = 0.0;              // Day low
    double open = 0.0;             // Opening price
    qint64 openInterest = 0;       // Open interest (for F&O)
    
    /**
     * @brief Create a blank separator row for organizing scrips
     * @return ScripData configured as a blank row
     */
    static ScripData createBlankRow() {
        ScripData blank;
        blank.isBlankRow = true;
        blank.symbol = "───────────────";  // Visual separator
        blank.token = -1;  // Invalid token for blank rows
        return blank;
    }
    
    /**
     * @brief Check if this is a valid tradeable scrip
     * @return true if token is valid and not a blank row
     */
    bool isValid() const {
        return token > 0 && !isBlankRow;
    }
};

/**
 * @brief Model for Market Watch data using Qt's Model/View framework
 * 
 * Manages a list of scrips with real-time price updates.
 * Supports sorting, blank rows, and efficient token-based updates.
 */
class MarketWatchModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * @brief Column indices for the market watch table
     */
    enum Column {
        COL_SYMBOL = 0,
        COL_LTP,
        COL_CHANGE,
        COL_CHANGE_PERCENT,
        COL_VOLUME,
        COL_BID,
        COL_ASK,
        COL_HIGH,
        COL_LOW,
        COL_OPEN,
        COL_OPEN_INTEREST,
        COL_COUNT  // Always last - total column count
    };

    explicit MarketWatchModel(QObject *parent = nullptr);
    virtual ~MarketWatchModel() = default;

    // Required QAbstractTableModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Data management
    void addScrip(const ScripData &scrip);
    void insertScrip(int position, const ScripData &scrip);
    void removeScrip(int row);
    void moveRow(int sourceRow, int targetRow);
    void clearAll();
    int findScrip(const QString &symbol) const;
    int findScripByToken(int token) const;
    
    // Blank row support
    void insertBlankRow(int position = -1);
    bool isBlankRow(int row) const;
    
    // Data access
    const ScripData& getScripAt(int row) const;
    ScripData& getScripAt(int row);
    
    // Price updates
    void updatePrice(int row, double ltp, double change, double changePercent);
    void updateVolume(int row, qint64 volume);
    void updateBidAsk(int row, double bid, double ask);
    void updateHighLow(int row, double high, double low);
    void updateOpenInterest(int row, qint64 oi);
    
    // Batch updates for efficiency
    void updateScripData(int row, const ScripData &scrip);
    
    // Statistics
    int scripCount() const;  // Count excluding blank rows
    int totalRowCount() const { return m_scrips.count(); }

signals:
    void scripAdded(int row, const ScripData &scrip);
    void scripRemoved(int row);
    void priceUpdated(int row, double ltp, double change);

private:
    QList<ScripData> m_scrips;
    QStringList m_headers;
    
    // Helper to emit data changed for a specific cell
    void emitCellChanged(int row, int column);
};

#endif // MARKETWATCHMODEL_H
