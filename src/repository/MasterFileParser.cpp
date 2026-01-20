#include "repository/MasterFileParser.h"
#include <QDebug>

// Helper function to remove surrounding quotes from field values
static QString trimQuotes(const QStringRef &str) {
  QStringRef trimmed = str.trimmed();
  if (trimmed.startsWith('"') && trimmed.endsWith('"') &&
      trimmed.length() >= 2) {
    return trimmed.mid(1, trimmed.length() - 2).toString();
  }
  return trimmed.toString();
}

bool MasterFileParser::parseLine(const QStringRef &line, const QString &exchange,
                                 MasterContract &contract) {
  // Split by pipe delimiter without allocating new strings for each field
  QVector<QStringRef> fields;
  int start = 0;
  int end = 0;
  while ((end = line.indexOf('|', start)) != -1) {
    fields.append(line.mid(start, end - start));
    start = end + 1;
  }
  fields.append(line.mid(start, line.length() - start));

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

bool MasterFileParser::parseNSECM(const QVector<QStringRef> &fields,
                                  MasterContract &contract) {
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
  if (!ok)
    return false;

  contract.instrumentType = fields[2].toInt();
  contract.name = trimQuotes(fields[3]);
  contract.description = trimQuotes(fields[4]);
  contract.series = trimQuotes(fields[5]);
  contract.nameWithSeries = trimQuotes(fields[6]);
  contract.instrumentID = trimQuotes(fields[7]);
  contract.priceBandHigh = fields[8].toDouble();
  contract.priceBandLow = fields[9].toDouble();
  contract.freezeQty = fields[10].toInt();
  contract.tickSize = fields[11].toDouble();
  contract.lotSize = fields[12].toInt();
  contract.multiplier = fields[13].toInt();

  // Field 15 (index 14) is usually a short name/alias
  // Field 19 (index 18) is the full human-readable DisplayName
  if (fields.size() >= 19) {
    contract.displayName = trimQuotes(fields[18]);
  } else {
    contract.displayName = trimQuotes(fields[14]);
  }

  contract.isin = trimQuotes(fields[15]);

  if (fields.size() >= 17) {
    contract.priceNumerator = fields[16].toInt();
  }
  if (fields.size() >= 18) {
    contract.priceDenominator = fields[17].toInt();
  }

  return true;
}

bool MasterFileParser::parseNSEFO(const QVector<QStringRef> &fields,
                                  MasterContract &contract) {
  // NSEFO format has TWO different layouts:
  //
  // OPTIONS (OPTSTK, OPTIDX): 19+ fields
  // 14: AssetToken, 15: UnderlyingName, 16: Expiry, 17: StrikePrice, 18:
  // OptionType, 19: DisplayName
  //
  // FUTURES (FUTSTK, FUTIDX): 17+ fields (NO strike price field!)
  // 14: AssetToken (-1 typically), 15: UnderlyingName, 16: Expiry, 17:
  // DisplayName, 18-19: Other

  if (fields.size() < 17) {
    return false;
  }

  bool ok;
  contract.exchange = "NSEFO";
  contract.exchangeInstrumentID = fields[1].toLongLong(&ok);
  if (!ok)
    return false;

  contract.instrumentType = fields[2].toInt();
  contract.name = trimQuotes(fields[3]);
  contract.description = trimQuotes(fields[4]);
  contract.series = trimQuotes(fields[5]);
  contract.nameWithSeries = trimQuotes(fields[6]);
  contract.instrumentID = trimQuotes(fields[7]);
  contract.priceBandHigh = fields[8].toDouble();
  contract.priceBandLow = fields[9].toDouble();
  contract.freezeQty = fields[10].toInt();
  contract.tickSize = fields[11].toDouble();
  contract.lotSize = fields[12].toInt();
  contract.multiplier = fields[13].toInt();
  contract.assetToken = fields[14].toLongLong();
  // field[15] = UnderlyingName/InstrumentID (not stored)
  contract.expiryDate = trimQuotes(fields[16]);

  // Check if this is OPTIONS or FUTURES or SPREADS based on instrumentType
  bool isOption = (contract.instrumentType == 2);
  bool isSpread = (contract.instrumentType == 4);

  if (isOption && fields.size() >= 20) {
    // OPTIONS (23 fields): 17=Strike, 18=OptionType, 19=DisplayName, 20=Num,
    // 21=Den
    contract.strikePrice = fields[17].toDouble();
    contract.optionType = fields[18].toInt();
    contract.displayName = trimQuotes(fields[19]);

    if (fields.size() >= 22) {
      contract.priceNumerator = fields[20].toInt();
      contract.priceDenominator = fields[21].toInt();
    }
    if (fields.size() >= 23) {
      contract.isin = trimQuotes(fields[22]);
    }
  } else if (isSpread && fields.size() >= 18) {
    // SPREADS (21 fields): 17=DisplayName, 18=Num, 19=Den
    contract.strikePrice = 0.0;
    contract.optionType = 0;
    contract.displayName = trimQuotes(fields[17]);
    if (fields.size() >= 20) {
      contract.priceNumerator = fields[18].toInt();
      contract.priceDenominator = fields[19].toInt();
    }
    if (fields.size() >= 21) {
      contract.isin = trimQuotes(fields[20]);
    }
  } else {
    // FUTURES (21 fields): 17=DisplayName, 18=Num, 19=Den
    contract.strikePrice = 0.0;
    contract.optionType = 0;
    contract.displayName = trimQuotes(fields[17]);
    if (fields.size() >= 20) {
      contract.priceNumerator = fields[18].toInt();
      contract.priceDenominator = fields[19].toInt();
    }
    if (fields.size() >= 21) {
      contract.isin = trimQuotes(fields[20]);
    }
  }

  // Convert expiry date to DDMMMYYYY format
  // Handles both YYYYMMDD ("20241226") and ISO ("2024-12-26T14:30:00") formats
  if (!contract.expiryDate.isEmpty()) {
    QString year, month, day;

      int tIdx = contract.expiryDate.indexOf('T');
      if (tIdx != -1) {
        int d1 = contract.expiryDate.indexOf('-');
        int d2 = contract.expiryDate.indexOf('-', d1 + 1);
        if (d1 != -1 && d2 != -1 && d2 < tIdx) {
          year = contract.expiryDate.mid(0, d1);
          month = contract.expiryDate.mid(d1 + 1, d2 - d1 - 1);
          day = contract.expiryDate.mid(d2 + 1, tIdx - d2 - 1);
        }
      } else if (contract.expiryDate.length() == 8 &&
               contract.expiryDate.at(0).isDigit()) {
      // YYYYMMDD format: 20241226
      year = contract.expiryDate.mid(0, 4);
      month = contract.expiryDate.mid(4, 2);
      day = contract.expiryDate.mid(6, 2);
    }

    // Convert to DDMMMYYYY
    if (!year.isEmpty() && !month.isEmpty() && !day.isEmpty()) {
      QStringList months = {"",    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      int monthNum = month.toInt();
      if (monthNum >= 1 && monthNum <= 12) {
        contract.expiryDate = day + months[monthNum] + year;
      }
    }
  }

  return true;
}

bool MasterFileParser::parseBSECM(const QVector<QStringRef> &fields,
                                  MasterContract &contract) {
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
  if (!ok)
    return false;

  contract.instrumentType = fields[2].toInt();
  contract.name = trimQuotes(fields[3]);
  contract.description = trimQuotes(fields[4]);
  contract.series = trimQuotes(fields[5]);
  contract.nameWithSeries = trimQuotes(fields[6]);
  contract.instrumentID = trimQuotes(fields[7]);
  contract.priceBandHigh = fields[8].toDouble();
  contract.priceBandLow = fields[9].toDouble();
  contract.freezeQty = fields[10].toInt();
  contract.tickSize = fields[11].toDouble();
  contract.lotSize = fields[12].toInt();
  contract.multiplier = fields[13].toInt();

  // Field 15 (index 14) is usually a short name/alias
  // Field 19 (index 18) is the full human-readable DisplayName
  if (fields.size() >= 19) {
    contract.displayName = trimQuotes(fields[18]);
  } else {
    contract.displayName = trimQuotes(fields[14]);
  }

  contract.isin = trimQuotes(fields[15]);

  if (fields.size() >= 17) {
    contract.priceNumerator = fields[16].toInt();
  }
  if (fields.size() >= 18) {
    contract.priceDenominator = fields[17].toInt();
  }

  return true;
}

bool MasterFileParser::parseBSEFO(const QVector<QStringRef> &fields,
                                  MasterContract &contract) {
  // BSEFO format has TWO different layouts (same as NSEFO):
  //
  // OPTIONS (IO, SO, etc): 19+ fields
  // 14: AssetToken, 15: UnderlyingName, 16: Expiry, 17: DisplayName, 18:
  // StrikePrice, 19: OptionType
  //
  // FUTURES (IF, etc): 17+ fields (NO strike price field!)
  // 14: AssetToken (-1 typically), 15: UnderlyingName, 16: Expiry, 17:
  // DisplayName, 18-19: Other

  if (fields.size() < 17) {
    return false;
  }

  bool ok;
  contract.exchange = "BSEFO";
  contract.exchangeInstrumentID = fields[1].toLongLong(&ok);
  if (!ok)
    return false;

  contract.instrumentType = fields[2].toInt();
  contract.name = trimQuotes(fields[3]);
  contract.description = trimQuotes(fields[4]);
  contract.series = trimQuotes(fields[5]);
  contract.nameWithSeries = trimQuotes(fields[6]);
  contract.instrumentID = trimQuotes(fields[7]);
  contract.priceBandHigh = fields[8].toDouble();
  contract.priceBandLow = fields[9].toDouble();
  contract.freezeQty = fields[10].toInt();
  contract.tickSize = fields[11].toDouble();
  contract.lotSize = fields[12].toInt();
  contract.multiplier = fields[13].toInt();
  contract.assetToken = fields[14].toLongLong();
  // field[15] = UnderlyingName/InstrumentID (not stored)
  contract.expiryDate = trimQuotes(fields[16]);

  // Check if this is OPTIONS, FUTURES or SPREADS based on instrumentType
  // BSE: 1=IF/SF (Futures), 2=IO/SO (Options), 4=SPD (Spreads)
  bool isOption = (contract.instrumentType == 2);
  bool isSpread = (contract.instrumentType == 4);

  if (isOption && fields.size() >= 19) {
    // OPTIONS (19-23 fields): 17=Strike, 18=OptionType, 19=DisplayName (if
    // present), 20-21=Num/Den
    contract.strikePrice = fields[17].toDouble();

    // Handle OptionType being numeric or string (CE/PE)
    QString optStr = trimQuotes(fields[18]);
    bool ok;
    int optTypeVal = optStr.toInt(&ok);
    if (ok) {
      contract.optionType = optTypeVal;
    } else {
      optStr = optStr.toUpper();
      if (optStr == "CE")
        contract.optionType = 1; // BSE CE
      else if (optStr == "PE")
        contract.optionType = 2; // BSE PE
      else
        contract.optionType = 0;
    }
    if (fields.size() >= 20) {
      contract.displayName = trimQuotes(fields[19]);
    }

    if (fields.size() >= 22) {
      contract.priceNumerator = fields[20].toInt();
      contract.priceDenominator = fields[21].toInt();
    }
    if (fields.size() >= 23) {
      contract.isin = trimQuotes(fields[22]);
    }
  } else if (isSpread && fields.size() >= 18) {
    // SPREADS (21 fields): 17=DisplayName, 18=Num, 19=Den
    contract.strikePrice = 0.0;
    contract.optionType = 0;
    contract.displayName = trimQuotes(fields[17]);
    if (fields.size() >= 20) {
      contract.priceNumerator = fields[18].toInt();
      contract.priceDenominator = fields[19].toInt();
    }
    if (fields.size() >= 21) {
      contract.isin = trimQuotes(fields[20]);
    }
  } else {
    // FUTURES (21 fields): 17=DisplayName, 18=Num, 19=Den
    contract.strikePrice = 0.0;
    contract.optionType = 0;
    contract.displayName = trimQuotes(fields[17]);
    if (fields.size() >= 20) {
      contract.priceNumerator = fields[18].toInt();
      contract.priceDenominator = fields[19].toInt();
    }
    if (fields.size() >= 21) {
      contract.isin = trimQuotes(fields[20]);
    }
  }

  // Convert expiry date to DDMMMYYYY format
  // Handles both YYYYMMDD ("20241226") and ISO ("2024-12-26T14:30:00") formats
  if (!contract.expiryDate.isEmpty()) {
    QString year, month, day;

      int tIdx = contract.expiryDate.indexOf('T');
      if (tIdx != -1) {
        int d1 = contract.expiryDate.indexOf('-');
        int d2 = contract.expiryDate.indexOf('-', d1 + 1);
        if (d1 != -1 && d2 != -1 && d2 < tIdx) {
          year = contract.expiryDate.mid(0, d1);
          month = contract.expiryDate.mid(d1 + 1, d2 - d1 - 1);
          day = contract.expiryDate.mid(d2 + 1, tIdx - d2 - 1);
        }
      } else if (contract.expiryDate.length() == 8 &&
               contract.expiryDate.at(0).isDigit()) {
      // YYYYMMDD format: 20241226
      year = contract.expiryDate.mid(0, 4);
      month = contract.expiryDate.mid(4, 2);
      day = contract.expiryDate.mid(6, 2);
    }

    // Convert to DDMMMYYYY
    if (!year.isEmpty() && !month.isEmpty() && !day.isEmpty()) {
      QStringList months = {"",    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      int monthNum = month.toInt();
      if (monthNum >= 1 && monthNum <= 12) {
        contract.expiryDate = day + months[monthNum] + year;
      }
    }
  }

  return true;
}

QString MasterFileParser::convertOptionType(int32_t optionType) {
  switch (optionType) {
  case 1:
    return "CE";
  case 2:
    return "PE";
  default:
    return "XX";
  }
}
