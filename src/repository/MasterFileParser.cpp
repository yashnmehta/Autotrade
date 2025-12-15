#include "repository/MasterFileParser.h"
#include <QDebug>

bool MasterFileParser::parseLine(const QString& line, const QString& exchange, 
                                 MasterContract& contract) {
    // Split by pipe delimiter
    QStringList fields = line.split('|');
    
    if (fields.isEmpty()) {
        return false;
    }
    
    // Route to appropriate parser based on exchange
    if (exchange == "NSECM") {
        return parseNSECM(fields, contract);
    } else if (exchange == "NSEFO") {
        return parseNSEFO(fields, contract);
    } else if (exchange == "BSECM") {
        return parseBSECM(fields, contract);
    } else if (exchange == "BSEFO") {
        return parseBSEFO(fields, contract);
    }
    
    return false;
}

bool MasterFileParser::parseNSECM(const QStringList& fields, MasterContract& contract) {
    // NSECM format (17 fields):
    // 0: ExchangeSegment (always 1 for NSECM)
    // 1: ExchangeInstrumentID (token)
    // 2: InstrumentType
    // 3: Name (symbol)
    // 4: Description
    // 5: Series
    // 6: NameWithSeries
    // 7: InstrumentID
    // 8: PriceBandHigh
    // 9: PriceBandLow
    // 10: FreezeQty
    // 11: TickSize
    // 12: LotSize
    // 13: Multiplier
    // 14: DisplayName
    // 15: ISIN
    // 16: PriceNumerator
    // (17: PriceDenominator - might be missing in some files)
    
    if (fields.size() < 16) {
        return false;
    }
    
    bool ok;
    contract.exchange = "NSECM";
    contract.exchangeInstrumentID = fields[1].toLongLong(&ok);
    if (!ok) return false;
    
    contract.instrumentType = fields[2].toInt();
    contract.name = fields[3];
    contract.description = fields[4];
    contract.series = fields[5];
    contract.nameWithSeries = fields[6];
    contract.instrumentID = fields[7];
    contract.priceBandHigh = fields[8].toDouble();
    contract.priceBandLow = fields[9].toDouble();
    contract.freezeQty = fields[10].toInt();
    contract.tickSize = fields[11].toDouble();
    contract.lotSize = fields[12].toInt();
    contract.multiplier = fields[13].toInt();
    contract.displayName = fields[14];
    contract.isin = fields[15];
    
    if (fields.size() >= 17) {
        contract.priceNumerator = fields[16].toInt();
    }
    if (fields.size() >= 18) {
        contract.priceDenominator = fields[17].toInt();
    }
    
    return true;
}

bool MasterFileParser::parseNSEFO(const QStringList& fields, MasterContract& contract) {
    // NSEFO format (19+ fields):
    // 0: ExchangeSegment (2 for NSEFO)
    // 1: ExchangeInstrumentID (token)
    // 2: InstrumentType
    // 3: Name (underlying symbol)
    // 4: Description
    // 5: Series (OPTSTK, OPTIDX, FUTSTK, FUTIDX)
    // 6: NameWithSeries
    // 7: InstrumentID
    // 8: PriceBandHigh
    // 9: PriceBandLow
    // 10: FreezeQty
    // 11: TickSize
    // 12: LotSize
    // 13: Multiplier
    // 14: UnderlyingToken (AssetToken)
    // 15: UnderlyingInstrumentID
    // 16: ExpiryDate (ISO format: YYYYMMDD)
    // 17: StrikePrice
    // 18: OptionType (1=CE, 2=PE, 3/4=XX)
    // 19: DisplayName
    // 20: ISIN (optional)
    // 21: PriceNumerator (optional)
    // 22: PriceDenominator (optional)
    
    if (fields.size() < 19) {
        return false;
    }
    
    bool ok;
    contract.exchange = "NSEFO";
    contract.exchangeInstrumentID = fields[1].toLongLong(&ok);
    if (!ok) return false;
    
    contract.instrumentType = fields[2].toInt();
    contract.name = fields[3];
    contract.description = fields[4];
    contract.series = fields[5];
    contract.nameWithSeries = fields[6];
    contract.instrumentID = fields[7];
    contract.priceBandHigh = fields[8].toDouble();
    contract.priceBandLow = fields[9].toDouble();
    contract.freezeQty = fields[10].toInt();
    contract.tickSize = fields[11].toDouble();
    contract.lotSize = fields[12].toInt();
    contract.multiplier = fields[13].toInt();
    contract.assetToken = fields[14].toLongLong();
    // field[15] = UnderlyingInstrumentID (not stored)
    contract.expiryDate = fields[16];  // ISO format YYYYMMDD
    contract.strikePrice = fields[17].toDouble();
    contract.optionType = fields[18].toInt();
    
    if (fields.size() >= 20) {
        contract.displayName = fields[19];
    }
    if (fields.size() >= 21) {
        contract.isin = fields[20];
    }
    if (fields.size() >= 22) {
        contract.priceNumerator = fields[21].toInt();
    }
    if (fields.size() >= 23) {
        contract.priceDenominator = fields[22].toInt();
    }
    
    // Convert expiry date from YYYYMMDD to DDMMMYYYY format
    // Example: "20241226" -> "26DEC2024"
    if (contract.expiryDate.length() == 8) {
        QString year = contract.expiryDate.mid(0, 4);
        QString month = contract.expiryDate.mid(4, 2);
        QString day = contract.expiryDate.mid(6, 2);
        
        // Convert month number to abbreviation
        QStringList months = {"", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        int monthNum = month.toInt();
        if (monthNum >= 1 && monthNum <= 12) {
            contract.expiryDate = day + months[monthNum] + year;
        }
    }
    
    return true;
}

bool MasterFileParser::parseBSECM(const QStringList& fields, MasterContract& contract) {
    // BSECM format (18 fields):
    // Similar to NSECM but includes ScripCode
    // 0: ExchangeSegment (11 for BSECM)
    // 1: ExchangeInstrumentID (token)
    // 2: InstrumentType
    // 3: Name (symbol)
    // 4: Description
    // 5: Series
    // 6: NameWithSeries
    // 7: InstrumentID
    // 8: PriceBandHigh
    // 9: PriceBandLow
    // 10: FreezeQty
    // 11: TickSize
    // 12: LotSize
    // 13: Multiplier
    // 14: DisplayName
    // 15: ISIN
    // 16: PriceNumerator
    // 17: PriceDenominator
    // (Additional field: ScripCode - position varies)
    
    if (fields.size() < 16) {
        return false;
    }
    
    bool ok;
    contract.exchange = "BSECM";
    contract.exchangeInstrumentID = fields[1].toLongLong(&ok);
    if (!ok) return false;
    
    contract.instrumentType = fields[2].toInt();
    contract.name = fields[3];
    contract.description = fields[4];
    contract.series = fields[5];
    contract.nameWithSeries = fields[6];
    contract.instrumentID = fields[7];
    contract.priceBandHigh = fields[8].toDouble();
    contract.priceBandLow = fields[9].toDouble();
    contract.freezeQty = fields[10].toInt();
    contract.tickSize = fields[11].toDouble();
    contract.lotSize = fields[12].toInt();
    contract.multiplier = fields[13].toInt();
    contract.displayName = fields[14];
    contract.isin = fields[15];
    
    if (fields.size() >= 17) {
        contract.priceNumerator = fields[16].toInt();
    }
    if (fields.size() >= 18) {
        contract.priceDenominator = fields[17].toInt();
    }
    
    return true;
}

bool MasterFileParser::parseBSEFO(const QStringList& fields, MasterContract& contract) {
    // BSEFO format (similar to NSEFO):
    // 0: ExchangeSegment (12 for BSEFO)
    // 1: ExchangeInstrumentID (token)
    // 2: InstrumentType
    // 3: Name (underlying symbol)
    // 4: Description
    // 5: Series
    // 6: NameWithSeries
    // 7: InstrumentID
    // 8: PriceBandHigh
    // 9: PriceBandLow
    // 10: FreezeQty
    // 11: TickSize
    // 12: LotSize
    // 13: Multiplier
    // 14: UnderlyingToken (AssetToken)
    // 15: UnderlyingInstrumentID
    // 16: ExpiryDate (ISO format: YYYYMMDD)
    // 17: StrikePrice
    // 18: OptionType (1=CE, 2=PE, 3/4=XX)
    // 19: DisplayName
    // 20: ISIN (optional)
    
    if (fields.size() < 19) {
        return false;
    }
    
    bool ok;
    contract.exchange = "BSEFO";
    contract.exchangeInstrumentID = fields[1].toLongLong(&ok);
    if (!ok) return false;
    
    contract.instrumentType = fields[2].toInt();
    contract.name = fields[3];
    contract.description = fields[4];
    contract.series = fields[5];
    contract.nameWithSeries = fields[6];
    contract.instrumentID = fields[7];
    contract.priceBandHigh = fields[8].toDouble();
    contract.priceBandLow = fields[9].toDouble();
    contract.freezeQty = fields[10].toInt();
    contract.tickSize = fields[11].toDouble();
    contract.lotSize = fields[12].toInt();
    contract.multiplier = fields[13].toInt();
    contract.assetToken = fields[14].toLongLong();
    // field[15] = UnderlyingInstrumentID (not stored)
    contract.expiryDate = fields[16];  // ISO format YYYYMMDD
    contract.strikePrice = fields[17].toDouble();
    contract.optionType = fields[18].toInt();
    
    if (fields.size() >= 20) {
        contract.displayName = fields[19];
    }
    if (fields.size() >= 21) {
        contract.isin = fields[20];
    }
    
    // Convert expiry date from YYYYMMDD to DDMMMYYYY format
    if (contract.expiryDate.length() == 8) {
        QString year = contract.expiryDate.mid(0, 4);
        QString month = contract.expiryDate.mid(4, 2);
        QString day = contract.expiryDate.mid(6, 2);
        
        QStringList months = {"", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        int monthNum = month.toInt();
        if (monthNum >= 1 && monthNum <= 12) {
            contract.expiryDate = day + months[monthNum] + year;
        }
    }
    
    return true;
}

QString MasterFileParser::convertOptionType(int32_t optionType) {
    switch (optionType) {
        case 1: return "CE";
        case 2: return "PE";
        default: return "XX";
    }
}
