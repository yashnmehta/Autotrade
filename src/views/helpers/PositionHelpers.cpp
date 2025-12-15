#include "views/helpers/PositionHelpers.h"
#include "views/PositionWindow.h"  // For Position struct
#include <QDebug>
#include <QLocale>

bool PositionHelpers::parsePositionFromTSV(const QString &line, Position &position)
{
    QStringList fields = line.split('\t');
    
    // Need at least Symbol, SeriesExpiry, BuyQty, SellQty
    if (fields.size() < 4) {
        return false;
    }
    
    QString symbol = fields.at(0).trimmed();
    QString seriesExpiry = fields.at(1).trimmed();
    
    if (symbol.isEmpty() || symbol.startsWith("─")) {
        return false;
    }
    
    // Populate basic fields
    position.symbol = symbol;
    position.seriesExpiry = seriesExpiry;
    
    // Parse quantities
    if (fields.size() >= 4) {
        position.buyQty = fields.at(2).toInt();
        position.sellQty = fields.at(3).toInt();
    }
    
    // Parse prices and P&L
    if (fields.size() >= 8) {
        position.netPrice = fields.at(4).toDouble();
        position.markPrice = fields.at(5).toDouble();
        position.mtmGainLoss = fields.at(6).toDouble();
        position.mtmMargin = fields.at(7).toDouble();
    }
    
    // Parse values
    if (fields.size() >= 10) {
        position.buyValue = fields.at(8).toDouble();
        position.sellValue = fields.at(9).toDouble();
    }
    
    // Parse metadata
    if (fields.size() >= 15) {
        position.exchange = fields.at(10).trimmed();
        position.segment = fields.at(11).trimmed();
        position.user = fields.at(12).trimmed();
        position.client = fields.at(13).trimmed();
        position.periodicity = fields.at(14).trimmed();
    }
    
    qDebug() << "[PositionHelpers] Parsed position:" << symbol << "Net Qty:" << calculateNetQty(position);
    return true;
}

QString PositionHelpers::formatPositionToTSV(const Position &position)
{
    // Format: Symbol\tSeriesExpiry\tBuyQty\tSellQty\tNetPrice\tMarkPrice\tMTM\tMargin\tBuyValue\tSellValue\tExchange\tSegment\tUser\tClient\tPeriodicity
    return QString("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11\t%12\t%13\t%14\t%15")
            .arg(position.symbol)
            .arg(position.seriesExpiry)
            .arg(position.buyQty)
            .arg(position.sellQty)
            .arg(position.netPrice, 0, 'f', 2)
            .arg(position.markPrice, 0, 'f', 2)
            .arg(position.mtmGainLoss, 0, 'f', 2)
            .arg(position.mtmMargin, 0, 'f', 2)
            .arg(position.buyValue, 0, 'f', 2)
            .arg(position.sellValue, 0, 'f', 2)
            .arg(position.exchange)
            .arg(position.segment)
            .arg(position.user)
            .arg(position.client)
            .arg(position.periodicity);
}

bool PositionHelpers::isValidPosition(const Position &position)
{
    return !position.symbol.isEmpty() && !position.symbol.startsWith("─");
}

int PositionHelpers::calculateNetQty(const Position &position)
{
    return position.buyQty - position.sellQty;
}

double PositionHelpers::calculateTotalPnL(const QList<Position> &positions)
{
    double totalPnL = 0.0;
    
    for (const Position &pos : positions) {
        totalPnL += pos.mtmGainLoss;
    }
    
    return totalPnL;
}

double PositionHelpers::calculateTotalMargin(const QList<Position> &positions)
{
    double totalMargin = 0.0;
    
    for (const Position &pos : positions) {
        totalMargin += pos.mtmMargin;
    }
    
    return totalMargin;
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

bool PositionHelpers::isSquaredOff(const Position &position)
{
    return calculateNetQty(position) == 0;
}
