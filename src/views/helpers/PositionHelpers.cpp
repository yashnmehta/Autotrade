#include "views/helpers/PositionHelpers.h"
#include "views/PositionWindow.h"  // For PositionData struct
#include <QDebug>
#include <QLocale>

bool PositionHelpers::parsePositionFromTSV(const QString &line, PositionData &position)
{
    QStringList fields = line.split('\t');
    
    // Need at least Symbol, Name, BuyQty, SellQty
    if (fields.size() < 4) {
        return false;
    }
    
    QString symbol = fields.at(0).trimmed();
    QString name = fields.at(1).trimmed();
    
    if (symbol.isEmpty() || symbol.startsWith("─")) {
        return false;
    }
    
    // Populate basic fields
    position.symbol = symbol;
    position.name = name;
    
    // Parse quantities
    if (fields.size() >= 4) {
        position.buyQty = fields.at(2).toInt();
        position.sellQty = fields.at(3).toInt();
    }
    
    // Parse prices and P&L
    if (fields.size() >= 8) {
        position.netPrice = fields.at(4).toDouble();
        position.marketPrice = fields.at(5).toDouble();
        position.mtm = fields.at(6).toDouble();
    }
    
    // Parse values
    if (fields.size() >= 10) {
        position.buyVal = fields.at(8).toDouble();
        position.sellVal = fields.at(9).toDouble();
    }
    
    // Parse metadata
    if (fields.size() >= 11) {
        position.exchange = fields.at(10).trimmed();
    }
    
    qDebug() << "[PositionHelpers] Parsed position:" << symbol << "Net Qty:" << calculateNetQty(position);
    return true;
}

QString PositionHelpers::formatPositionToTSV(const PositionData &position)
{
    // Format: Symbol\tName\tBuyQty\tSellQty\tNetPrice\tMarketPrice\tMTM\tMargin\tBuyVal\tSellVal\tExchange
    return QString("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11")
            .arg(position.symbol)
            .arg(position.name)
            .arg(position.buyQty)
            .arg(position.sellQty)
            .arg(position.netPrice, 0, 'f', 2)
            .arg(position.marketPrice, 0, 'f', 2)
            .arg(position.mtm, 0, 'f', 2)
            .arg(0.0, 0, 'f', 2) // Margin placeholder
            .arg(position.buyVal, 0, 'f', 2)
            .arg(position.sellVal, 0, 'f', 2)
            .arg(position.exchange);
}

bool PositionHelpers::isValidPosition(const PositionData &position)
{
    return !position.symbol.isEmpty() && !position.symbol.startsWith("─");
}

int PositionHelpers::calculateNetQty(const PositionData &position)
{
    return position.buyQty - position.sellQty;
}

double PositionHelpers::calculateTotalPnL(const QList<PositionData> &positions)
{
    double totalPnL = 0.0;
    
    for (const PositionData &pos : positions) {
        totalPnL += pos.mtm;
    }
    
    return totalPnL;
}

double PositionHelpers::calculateTotalMargin(const QList<PositionData> &positions)
{
    return 0.0;
}

QString PositionHelpers::formatPnL(double pnl)
{
    QLocale locale(QLocale::English);
    QString formatted = locale.toString(qAbs(pnl), 'f', 2);
    
    if (pnl > 0) {
        return "+" + formatted;
    } else if (pnl < 0) {
        return "-" + formatted;
    } else {
        return formatted;
    }
}

bool PositionHelpers::isSquaredOff(const PositionData &position)
{
    return calculateNetQty(position) == 0;
}
