#ifndef CONTRACT_DATA_H
#define CONTRACT_DATA_H

#include <QDate>
#include <QString>
#include <cstdint>


namespace XTS {
/**
 * @brief Instrument type codes from XTS API
 * Based on actual master file data analysis (131,009 records)
 */
enum class InstrumentType : int32_t {
  Future = 1, // Futures contracts
  Option = 2, // Options contracts
  Spread = 4, // Calendar/inter-commodity spreads
  Equity = 8  // Equity stocks (cash market)
};

/**
 * @brief Option type codes from XTS API
 * NSE uses 3/4, BSE may use 1/2 or 3/4 (dual encoding)
 */
enum class OptionType : int32_t {
  CE = 3,     // Call European (NSE standard)
  PE = 4,     // Put European (NSE standard)
  CE_BSE = 1, // Call (BSE alternate encoding)
  PE_BSE = 2  // Put (BSE alternate encoding)
};

/**
 * @brief Check if option type code represents a Call option
 * Accepts both NSE (3) and BSE (1) encodings
 */
static inline bool isCallOption(int32_t optType) {
  return optType == 1 || optType == 3;
}

/**
 * @brief Check if option type code represents a Put option
 * Accepts both NSE (4) and BSE (2) encodings
 */
static inline bool isPutOption(int32_t optType) {
  return optType == 2 || optType == 4;
}

/**
 * @brief Check if instrument type is an option
 */
static inline bool isOption(int32_t instType) {
  return instType == static_cast<int32_t>(InstrumentType::Option);
}

/**
 * @brief Check if instrument type is a future
 */
static inline bool isFuture(int32_t instType) {
  return instType == static_cast<int32_t>(InstrumentType::Future);
}

/**
 * @brief Check if instrument type is a spread
 */
static inline bool isSpread(int32_t instType) {
  return instType == static_cast<int32_t>(InstrumentType::Spread);
}
} // namespace XTS

/**
 * @brief Unified contract data structure for all segments (NSE/BSE CM/FO)
 *
 * This structure represents a standardized contract across all exchanges and
 * segments. It follows the Go implementation's ContractData structure for
 * consistency.
 *
 * Memory Layout Optimization:
 * - Grouped by type and usage pattern for cache efficiency
 * - Total size: ~200 bytes per contract
 */
struct ContractData {
  // ===== SECURITY MASTER DATA =====
  int64_t exchangeInstrumentID; // Exchange token (primary key)
  QString name;                 // Underlying symbol (e.g., MANAPPURAM, NIFTY)
  QString displayName; // Full name with strike (e.g., MANAPPURAM 27JAN2026 PE
                       // 372.5)
  QString description; // Contract description (e.g., MANAPPURAM26JAN372.5PE)
  QString series;      // Series type (OPTSTK, OPTIDX, FUTSTK, FUTIDX, EQ, etc.)
  QString scripCode;   // BSE scrip code (11-14 digits, BSE CM/FO only)
  int32_t instrumentType; // 1=Future, 2=Option, 4=Spread (BSE/NSE FO)

  // ===== TRADING PARAMETERS =====
  int32_t lotSize;   // Contract lot size
  int32_t freezeQty; // Freeze quantity (exchange-mandated limit)
  double tickSize;   // Minimum price movement

  // ===== PRICE BANDS =====
  double priceBandHigh; // Upper circuit limit
  double priceBandLow;  // Lower circuit limit

  // ===== F&O SPECIFIC FIELDS =====
  QString expiryDate;  // Expiry date (DDMMMYYYY format, e.g., 26DEC2024)
  QDate expiryDate_dt; // Parsed expiry date for sorting/comparison
  double timeToExpiry; // Pre-calculated time to expiry in years: (expiry_date -
                       // trade_date) / 365.0
  double strikePrice;  // Strike price (0 for futures, >0 for options)
  QString optionType;  // CE/PE/XX (Call/Put/NotApplicable)
  QString underlyingIndexName; // Name of underlying index (e.g., Nifty 50)
  int64_t assetToken;          // Underlying asset token

  // ===== LIVE MARKET DATA =====
  double ltp;       // Last Traded Price
  double open;      // Opening price
  double high;      // High price
  double low;       // Low price
  double close;     // Closing price
  double prevClose; // Previous close
  int64_t volume;   // Volume
  double bidPrice;  // Best bid price (WebSocket only)
  double askPrice;  // Best ask price (WebSocket only)

  // ===== MARGIN & GREEKS (F&O only) =====
  double spanMargin; // SPAN margin
  double aelMargin;  // AEL margin
  double iv;         // Implied volatility
  double delta;      // Delta (options)
  double gamma;      // Gamma (options)
  double vega;       // Vega (options)
  double theta;      // Theta (options)
  double rho;        // Rho (options)

  // Constructor with default values
  ContractData()
      : exchangeInstrumentID(0), lotSize(0), freezeQty(0), tickSize(0.0),
        priceBandHigh(0.0), priceBandLow(0.0), timeToExpiry(0.0),
        strikePrice(0.0), assetToken(0), ltp(0.0), open(0.0), high(0.0),
        low(0.0), close(0.0), prevClose(0.0), volume(0), bidPrice(0.0),
        askPrice(0.0), spanMargin(0.0), aelMargin(0.0), iv(0.0), delta(0.0),
        gamma(0.0), vega(0.0), theta(0.0), rho(0.0), instrumentType(0) {}
};

/**
 * @brief Raw master contract data parsed from master files
 *
 * This is the intermediate structure used when parsing master files.
 * It gets converted to ContractData for storage in repositories.
 */
struct MasterContract {
  QString exchange; // NSECM, NSEFO, BSECM, BSEFO
  int64_t exchangeInstrumentID;
  int32_t instrumentType;
  QString name;
  QString description;
  QString series;
  QString nameWithSeries;
  QString instrumentID;
  double priceBandHigh;
  double priceBandLow;
  int32_t freezeQty;
  double tickSize;
  int32_t lotSize;
  int32_t multiplier;
  QString displayName;
  QString isin;
  int32_t priceNumerator;
  int32_t priceDenominator;
  QString detailedName;

  // F&O Specific (from field positions 14-18 in NSEFO/BSEFO)
  QString expiryDate;  // Field 16 (ISO format or DDMMMYYYY)
  QDate expiryDate_dt; // Parsed expiry date
  double timeToExpiry; // Pre-calculated (expiry_date - trade_date) / 365.0 in
                       // years
  double strikePrice;  // Field 17
  int32_t optionType;  // Field 18 (1=CE, 2=PE, 3/4=XX)
  QString underlyingIndexName; // Field 15 (UnderlyingIndexName)
  int64_t assetToken;          // Field 14 (underlying asset token)

  // Constructor
  MasterContract()
      : exchangeInstrumentID(0), instrumentType(0), freezeQty(0), tickSize(0.0),
        lotSize(0), multiplier(0), priceNumerator(0), priceDenominator(0),
        timeToExpiry(0.0), strikePrice(0.0), optionType(0), assetToken(0),
        priceBandHigh(0.0), priceBandLow(0.0) {}

  /**
   * @brief Convert MasterContract to ContractData
   */
  ContractData toContractData() const {
    ContractData data;
    data.exchangeInstrumentID = exchangeInstrumentID;
    data.name = name;
    data.displayName = displayName;
    data.description = description;
    data.series = series;
    data.lotSize = lotSize;
    data.freezeQty = freezeQty;
    data.tickSize = tickSize;
    data.priceBandHigh = priceBandHigh;
    data.priceBandLow = priceBandLow;
    data.expiryDate = expiryDate;
    data.expiryDate_dt = expiryDate_dt;
    data.timeToExpiry = timeToExpiry;
    data.strikePrice = strikePrice;
    data.underlyingIndexName = underlyingIndexName;
    data.assetToken = assetToken;
    data.instrumentType = instrumentType;

    // Convert optionType from int to string based on exchange and
    // instrumentType
    if (exchange == "NSECM" || exchange == "BSECM") {
      data.optionType = "EQ";
    } else {
      // F&O Segments
      if (instrumentType == 4) {
        data.optionType = "SPD";
      } else if (instrumentType == 2) {
        if (optionType == 3) {
          data.optionType = "CE";
        } else if (optionType == 4) {
          data.optionType = "PE";
        } else {
          data.optionType = "XX";
        }
      } else if (instrumentType == 1) {
        data.optionType = "FUT";
      } else {
        data.optionType = "XX";
      }
    }

    return data;
  }
};

#endif // CONTRACT_DATA_H
