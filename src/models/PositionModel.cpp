#include "models/PositionModel.h"
#include <QColor>
#include <QFont>
#include <QSet>

PositionModel::PositionModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_showSummary(false)
    , m_filterRowVisible(false)
{
}

int PositionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    int rows = m_positions.size();
    if (m_filterRowVisible) rows += 1;
    if (m_showSummary) rows += 1;
    return rows;
}

int PositionModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PositionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return QVariant();

    int row = index.row();
    int col = index.column();

    bool isFilterRow = m_filterRowVisible && (row == 0);
    bool isSummaryRow = m_showSummary && (row == rowCount() - 1);

    if (isFilterRow) {
        if (role == Qt::BackgroundRole) return QColor(240, 248, 255);
        if (role == Qt::UserRole) return "FILTER_ROW";
        return QVariant();
    }

    if (isSummaryRow && role == Qt::UserRole) return "SUMMARY_ROW";

    // Bounds checking to prevent crashes
    int dataRow = m_filterRowVisible ? row - 1 : row;
    if (!isSummaryRow && (dataRow < 0 || dataRow >= m_positions.size())) {
        return QVariant(); // Safe return for out-of-bounds access
    }
    
    const PositionData& pos = isSummaryRow ? m_summary : m_positions.at(dataRow);

    if (role == Qt::DisplayRole) {
        switch (col) {
            case ScripCode: return QString::number(pos.scripCode);
            case Symbol: return pos.symbol;
            case SeriesExpiry: return pos.seriesExpiry;
            case StrikePrice: return QString::number(pos.strikePrice, 'f', 2);
            case OptionType: return pos.optionType;
            case NetQty: return QString::number(pos.netQty);
            case MarketPrice: return QString::number(pos.marketPrice, 'f', 2);
            case MTMGL: return QString::number(pos.mtm, 'f', 2);
            case NetPrice: return QString::number(pos.netPrice, 'f', 2);
            case MTMVPos: return QString::number(pos.mtmvPos, 'f', 2);
            case TotalValue: return QString::number(pos.totalValue, 'f', 2);
            case BuyVal: return QString::number(pos.buyVal, 'f', 2);
            case SellVal: return QString::number(pos.sellVal, 'f', 2);
            case Exchange: return pos.exchange;
            case User: return pos.user;
            case Client: return pos.client;
            case Name: return pos.name;
            case InstrumentType: return pos.instrumentType;
            case InstrumentName: return pos.instrumentName;
            case ScripName: return pos.scripName;
            case BuyQty: return QString::number(pos.buyQty);
            case BuyLot: return QString::number(pos.buyLot, 'f', 2);
            case BuyWeight: return QString::number(pos.buyWeight, 'f', 2);
            case BuyAvg: return QString::number(pos.buyAvg, 'f', 2);
            case SellQty: return QString::number(pos.sellQty);
            case SellLot: return QString::number(pos.sellLot, 'f', 2);
            case SellWeight: return QString::number(pos.sellWeight, 'f', 2);
            case SellAvg: return QString::number(pos.sellAvg, 'f', 2);
            case NetLot: return QString::number(pos.netLot, 'f', 2);
            case NetWeight: return QString::number(pos.netWeight, 'f', 2);
            case NetVal: return QString::number(pos.netVal, 'f', 2);
            case ProductType: return pos.productType;
            case ClientGroup: return pos.clientGroup;
            case DPRRange: return QString::number(pos.dprRange, 'f', 2);
            case MaturityDate: return pos.maturityDate;
            case Yield: return QString::number(pos.yield, 'f', 2);
            case TotalQuantity: return QString::number(pos.totalQuantity);
            case TotalLot: return QString::number(pos.totalLot, 'f', 2);
            case TotalWeight: return QString::number(pos.totalWeight, 'f', 2);
            case Brokerage: return QString::number(pos.brokerage, 'f', 2);
            case NetMTM: return QString::number(pos.netMtm, 'f', 2);
            case NetValuePostExp: return QString::number(pos.netValPostExp, 'f', 2);
            case OptionFlag: return pos.optionFlag;
            case VarPercent: return QString::number(pos.varPercent, 'f', 2);
            case VarAmount: return QString::number(pos.varAmount, 'f', 2);
            case SMCategory: return pos.smCategory;
            case CfAvgPrice: return QString::number(pos.cfAvgPrice, 'f', 2);
            case ActualMTM: return QString::number(pos.actualMtm, 'f', 2);
            case UnsettledQty: return QString::number(pos.unsettledQty);
        }
    }
    else if (role == Qt::EditRole || role == Qt::UserRole) {
        switch (col) {
            case ScripCode: return (qlonglong)pos.scripCode;
            case Symbol: return pos.symbol;
            case SeriesExpiry: return pos.seriesExpiry;
            case StrikePrice: return pos.strikePrice;
            case OptionType: return pos.optionType;
            case NetQty: return pos.netQty;
            case MarketPrice: return pos.marketPrice;
            case MTMGL: return pos.mtm;
            case NetPrice: return pos.netPrice;
            case MTMVPos: return pos.mtmvPos;
            case TotalValue: return pos.totalValue;
            case BuyVal: return pos.buyVal;
            case SellVal: return pos.sellVal;
            case Exchange: return pos.exchange;
            case User: return pos.user;
            case Client: return pos.client;
            case Name: return pos.name;
            case InstrumentType: return pos.instrumentType;
            case InstrumentName: return pos.instrumentName;
            case ScripName: return pos.scripName;
            case BuyQty: return pos.buyQty;
            case BuyAvg: return pos.buyAvg;
            case SellQty: return pos.sellQty;
            case SellAvg: return pos.sellAvg;
            case ProductType: return pos.productType;
            case ClientGroup: return pos.clientGroup;
            case MaturityDate: return pos.maturityDate;
            case NetMTM: return pos.netMtm;
            case ActualMTM: return pos.actualMtm;
            case UnsettledQty: return pos.unsettledQty;
            default: return QVariant();
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        switch (col) {
            case ScripCode: case Symbol: case SeriesExpiry: case OptionType: case Exchange:
            case User: case Client: case Name: case InstrumentType: case InstrumentName:
            case ScripName: case ProductType: case ClientGroup: case MaturityDate:
            case OptionFlag: case SMCategory:
                return QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
            default:
                return QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
        }
    }
    else if (role == Qt::ForegroundRole) {
        if (col == MTMGL || col == NetMTM || col == ActualMTM) {
            double val = (col == MTMGL) ? pos.mtm : (col == NetMTM ? pos.netMtm : pos.actualMtm);
            return val > 0 ? QColor("#2e7d32") : // Premium Dark Green
                   val < 0 ? QColor("#c62828") : // Premium Dark Red
                   QColor(Qt::black);
        }
    }
    else if (role == Qt::BackgroundRole) {
        if (isSummaryRow) return QColor("#f5f5f5"); // Light grey for summary
        
        double val = pos.mtm;
        if (val > 0) return QColor("#e8f5e9"); // Very light premium green
        if (val < 0) return QColor("#ffebee"); // Very light premium red
    }
    else if (role == Qt::FontRole) {
        if (isSummaryRow) { QFont font; font.setBold(true); font.setPointSize(11); return font; }
    }

    return QVariant();
}

QVariant PositionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    static const QStringList headers = {
        "Scrip Code", "Symbol", "Ser/Exp", "Strike Price", "Option Type", "Net Qty",
        "Market Price", "MTM G/L", "Net Price", "MTMV Pos", "Total Value", "Buy Val", "Sell Val",
        "Exchange", "User", "Client", "Name", "Instrument Type", "Instrument Name",
        "Scrip Name", "Buy Qty", "Buy Lot", "Buy Weight", "Buy Avg.", "Sell Qty", "Sell Lot",
        "Sell Weight", "Sell Avg.", "Net Lot", "Net Weight", "Net Val", "Product Type",
        "Client Group", "DPR Range", "Maturity Date", "Yield", "Total Quantity",
        "Total Lot", "Total Weight", "Brokerage", "Net MTM", "Net Value Post Exp",
        "Option Flag", "VAR %", "VAR Amount", "SM Category", "CF Avg Price",
        "Actual MTM", "Unsettled Qty"
    };

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < headers.size()) {
        return headers.at(section);
    }
    return QVariant();
}

void PositionModel::setPositions(const QList<PositionData>& newPositions)
{
    // Use a unique key for matching: ScripCode + ProductType
    auto getKey = [](const PositionData& p) { 
        return QString("%1_%2").arg(p.scripCode).arg(p.productType); 
    };

    // Index mapping for current positions
    QMap<QString, int> existingIndices;
    for (int i = 0; i < m_positions.size(); ++i) {
        existingIndices[getKey(m_positions[i])] = i;
    }

    QList<PositionData> finalPositions;
    QSet<QString> handledKeys;
    bool anyChange = false;

    // 1. Update existing and find new positions
    for (const auto& newPos : newPositions) {
        QString key = getKey(newPos);
        handledKeys.insert(key);

        if (existingIndices.contains(key)) {
            int oldIdx = existingIndices[key];
            int modelRow = m_filterRowVisible ? oldIdx + 1 : oldIdx;
            
            // Update data
            m_positions[oldIdx] = newPos;
            
            // Notify view that this row changed
            emit dataChanged(index(modelRow, 0), index(modelRow, ColumnCount - 1));
        } else {
            // New position found
            finalPositions.append(newPos);
            anyChange = true;
        }
    }

    // 2. Handle Insertions (New Positions)
    if (!finalPositions.isEmpty()) {
        int startRow = m_positions.size();
        int modelStart = m_filterRowVisible ? startRow + 1 : startRow;
        
        // If summary is showing, it's always at the last actual row (rowCount - 1)
        // We need to insert BEFORE the summary row.
        int insertIdx = m_positions.size();
        
        beginInsertRows(QModelIndex(), modelStart, modelStart + finalPositions.size() - 1);
        m_positions.append(finalPositions);
        endInsertRows();
    }

    // 3. Handle Deletions (Removed Positions)
    for (int i = m_positions.size() - 1; i >= 0; --i) {
        QString key = getKey(m_positions[i]);
        if (!handledKeys.contains(key)) {
            int modelRow = m_filterRowVisible ? i + 1 : i;
            beginRemoveRows(QModelIndex(), modelRow, modelRow);
            m_positions.removeAt(i);
            endRemoveRows();
            anyChange = true;
        }
    }
}

void PositionModel::setSummary(const PositionData& summary)
{
    if (!m_showSummary) {
        // Showing summary for the first time - insert row
        int newRowIdx = rowCount();
        beginInsertRows(QModelIndex(), newRowIdx, newRowIdx);
        m_summary = summary;
        m_showSummary = true;
        endInsertRows();
    } else {
        // Update existing summary
        m_summary = summary;
        int summaryRow = rowCount() - 1;
        if (summaryRow >= 0) {
            emit dataChanged(index(summaryRow, 0), index(summaryRow, ColumnCount - 1));
        }
    }
}

void PositionModel::setFilterRowVisible(bool visible)
{
    if (m_filterRowVisible == visible) return;
    beginResetModel();
    m_filterRowVisible = visible;
    endResetModel();
}
