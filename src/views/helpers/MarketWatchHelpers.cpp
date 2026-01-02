#include "views/helpers/MarketWatchHelpers.h"
#include "models/MarketWatchModel.h"  // For ScripData struct
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

bool MarketWatchHelpers::parseScripFromTSV(const QString &line, ScripData &scrip)
{
    QStringList fields = line.split('\t');
    
    // Need at least Symbol, Exchange, Token
    if (fields.size() < 3) {
        return false;
    }
    
    QString symbol = fields.at(0).trimmed();
    QString exchange = fields.at(1).trimmed();
    bool ok;
    int token = fields.at(2).toInt(&ok);

    // If exchange is numeric, convert to name
    bool isNumeric;
    int exchangeId = exchange.toInt(&isNumeric);
    if (isNumeric) {
        exchange = RepositoryManager::getExchangeSegmentName(exchangeId);
    }
    
    // Validate basic fields
    if (symbol.isEmpty() || symbol.startsWith("─") || !ok || token <= 0) {
        return false;
    }
    
    // Populate basic fields
    scrip.symbol = symbol;
    scrip.exchange = exchange;
    scrip.token = token;

    scrip.isBlankRow = false;
    
    // Parse optional price data (fields 3-11)
    if (fields.size() >= 7) {
        scrip.ltp = fields.at(3).toDouble();
        scrip.change = fields.at(4).toDouble();
        scrip.changePercent = fields.at(5).toDouble();
        scrip.volume = fields.at(6).toLongLong();
    } else {
        scrip.ltp = 0.0;
        scrip.change = 0.0;
        scrip.changePercent = 0.0;
        scrip.volume = 0;
    }
    
    if (fields.size() >= 11) {
        scrip.bid = fields.at(7).toDouble();
        scrip.ask = fields.at(8).toDouble();
        scrip.high = fields.at(9).toDouble();
        scrip.low = fields.at(10).toDouble();
    } else {
        scrip.bid = 0.0;
        scrip.ask = 0.0;
        scrip.high = 0.0;
        scrip.low = 0.0;
    }
    
    if (fields.size() >= 12) {
        scrip.open = fields.at(11).toDouble();
    } else {
        scrip.open = 0.0;
    }
    
    scrip.openInterest = 0;  // Not in clipboard format
    
    qDebug() << "[MarketWatchHelpers] Parsed scrip:" << symbol << "token:" << token;
    return true;
}

QString MarketWatchHelpers::formatScripToTSV(const ScripData &scrip)
{
    // Format: Symbol\tExchange\tToken\tLTP\tChange\t%Change\tVolume\tBid\tAsk\tHigh\tLow\tOpen
    return QString("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11\t%12")
            .arg(scrip.symbol)
            .arg(scrip.exchange)
            .arg(scrip.token)
            .arg(scrip.ltp, 0, 'f', 2)
            .arg(scrip.change, 0, 'f', 2)
            .arg(scrip.changePercent, 0, 'f', 2)
            .arg(scrip.volume)
            .arg(scrip.bid, 0, 'f', 2)
            .arg(scrip.ask, 0, 'f', 2)
            .arg(scrip.high, 0, 'f', 2)
            .arg(scrip.low, 0, 'f', 2)
            .arg(scrip.open, 0, 'f', 2);
}

bool MarketWatchHelpers::isValidScrip(const ScripData &scrip)
{
    return !scrip.symbol.isEmpty() 
           && !scrip.exchange.isEmpty() 
           && scrip.token > 0
           && !scrip.isBlankRow;
}

bool MarketWatchHelpers::isBlankLine(const QString &line)
{
    QString trimmed = line.trimmed();
    
    // Check for empty or separator patterns
    if (trimmed.isEmpty() || trimmed.startsWith("─") || trimmed.startsWith("--")) {
        return true;
    }
    
    return false;
}

int MarketWatchHelpers::extractToken(const QString &line)
{
    QStringList fields = line.split('\t');
    
    if (fields.size() < 3) {
        return -1;
    }
    
    bool ok;
    int token = fields.at(2).toInt(&ok);
    
    return ok ? token : -1;
}

bool MarketWatchHelpers::savePortfolio(const QString &filename, const QList<ScripData> &scrips)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }

    QJsonArray scripArray;
    for (const auto &scrip : scrips) {
        QJsonObject scripObject;
        scripObject["symbol"] = scrip.symbol;
        scripObject["exchange"] = scrip.exchange;
        scripObject["token"] = scrip.token;
        scripObject["isBlankRow"] = scrip.isBlankRow;
        
        // Save minimal instrument details to help with identification/validation on load
        if (!scrip.isBlankRow) {
           scripObject["scripName"] = scrip.scripName;
           scripObject["instrumentName"] = scrip.instrumentName;
        }

        scripArray.append(scripObject);
    }

    QJsonDocument doc(scripArray);
    file.write(doc.toJson());
    file.close();

    return true;
}

bool MarketWatchHelpers::loadPortfolio(const QString &filename, QList<ScripData> &scrips)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading:" << filename;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "Invalid JSON in portfolio file:" << filename;
        return false;
    }

    QJsonArray scripArray = doc.array();
    scrips.clear();

    for (const auto &val : scripArray) {
        if (!val.isObject()) continue;
        
        QJsonObject obj = val.toObject();
        ScripData scrip;
        
        scrip.symbol = obj["symbol"].toString();
        scrip.exchange = obj["exchange"].toString();
        scrip.token = obj["token"].toInt();
        scrip.isBlankRow = obj["isBlankRow"].toBool();
        
        if (!scrip.isBlankRow) {
            scrip.scripName = obj["scripName"].toString();
            scrip.instrumentName = obj["instrumentName"].toString();
        } else {
             // Ensure blank row has visual separator if not explicitly saved/loaded
             if (scrip.symbol.isEmpty()) {
                 scrip.symbol = "───────────────";
             }
             scrip.token = -1;
        }

        scrips.append(scrip);
    }

    return true;
}
