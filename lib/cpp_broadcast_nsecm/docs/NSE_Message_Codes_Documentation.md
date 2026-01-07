# NSE Capital Market Trading System - Message Codes and Data Structures

## Document Overview
This document contains detailed information about NSE Capital Market Trading System message codes for building a minimal real-time feed handler. The focus is on message codes required for intraday book+trade view.

---

## Complete Message Codes by Category

### Core Market Data / Book / Ticker
- **7200** – BCAST_MBO_MBP_UPDATE (Market by Order + Market by Price update)
- **7201** – BCAST_MW_ROUND_ROBIN (Market Watch update)
- **7208** – BCAST_ONLY_MBP (Only Market by Price update)
- **7214** – BCAST_CALL_AUCTION_MBP (Call Auction MBP broadcast)
- **7215** – BCAST_CA_MW (Broadcast Call Auction Market Watch)
- **7210** – BCAST_CALL_AUCTION_ORD_CXL_UPDATE (Call Auction order cancel update)
- **18703** – BCAST_TICKER_AND_MKT_INDEX (Ticker and Market Index)

### Indices / Indicative Indices / Industry Indices
- **7207** – BCAST_INDICES (Multiple Index Broadcast)
- **7216** – BCAST_INDICES_VIX (Multiple Index Broadcast for India VIX)
- **8207** – BCAST_INDICATIVE_INDICES (Multiple Indicative Index Broadcast)
- **18201** – MARKET_STATS_REPORT_DATA (Security bhav copy – header, data, depository marker, trailer)
- **1836** – MKT_IDX_RPT_DATA (Index bhav copy)
- **7203** – BCAST_IND_INDICES (Broadcast Industry Indices)

### Security / Participant / Buyback / Turnover
- **18720** – BCAST_SECURITY_MSTR_CHG (Security master change)
- **18130** – BCAST_SECURITY_STATUS_CHG (Security status change)
- **18707** – BCAST_SECURITY_STATUS_CHG_PREOPEN (Security status change – preopen)
- **7306** – BCAST_PART_MSTR_CHG (Participant master change)
- **18708** – BCAST_BUY_BACK (Broadcast Buy Back information)
- **9010** – BCAST_TURNOVER_EXCEEDED (Turnover limit exceeded warning/deactivation)
- **9011** – BROADCAST_BROKER_REACTIVATED (Broker reactivated)

### System / Market Status / General Text
- **7206** – BCAST_SYSTEM_INFORMATION_OUT (System information / market status parameters)
- **6501** – BCAST_JRNL_VCT_MSG (General Broadcast Message – text, including Bhav copy start, etc.)
- **6511** – BC_OPEN_MESSAGE (Market open message)
- **6521** – BC_CLOSE_MESSAGE (Market close message)
- **6531** – BC_PREOPEN_SHUTDOWN_MSG (Market preopen message)
- **6571** – BC_NORMAL_MKT_PREOPEN_ENDED (Normal market preopen ended)
- **6583** – BC_CLOSING_START (Closing session start)
- **6584** – BC_CLOSING_END (Closing session end)
- **7764** – BC_SYMBOL_STATUS_CHANGE_ACTION (Security-level trading/market status change)

### Auction Broadcasts
- **18700** – BCAST_AUCTION_INQUIRY_OUT (Auction Activity Message – MS_AUCTION_INQ_DATA)
- **6581** – BC_AUCTION_STATUS_CHANGE (Auction Status Change + message)

---

## Data Types and Basic Structures

### Data Types Used
| Data Type | Size (Bytes) | Signed/Unsigned | Description |
|-----------|-------------|-----------------|-------------|
| CHAR | 1 | Signed | Character data |
| UINT | 2 | Unsigned | Unsigned integer |
| SHORT | 2 | Signed | Signed short |
| LONG | 4 | Signed | Signed long |
| UNSIGNED LONG | 4 | Unsigned | Unsigned long |
| LONG LONG | 8 | Signed | 64-bit signed integer |
| DOUBLE | 8 | Signed + Float | Double precision float |

### Common Message Headers

#### 1. MESSAGE_HEADER (40 bytes)
```c
struct MESSAGE_HEADER {
    SHORT TransactionCode;      // Transaction message number
    LONG LogTime;               // Set to zero when sending
    CHAR AlphaChar[2];          // First two characters of Symbol or blank
    LONG TraderId;              // User ID
    SHORT ErrorCode;            // Zero when sending, error code when receiving
    LONG LONG TimeStamp;        // Nanoseconds from 01-Jan-1980 00:00:00
    CHAR TimeStamp1[8];         // Time in jiffies from host
    CHAR TimeStamp2[8];         // Machine number from which packet is coming
    SHORT MessageLength;        // Total message length including header
};
```

#### 2. BCAST_HEADER (40 bytes) - Used for Broadcast Messages
```c
struct BCAST_HEADER {
    CHAR Reserved[4];           // Reserved field
    LONG LogTime;               // Time when message was generated
    CHAR AlphaChar[2];          // First two characters of Symbol
    SHORT TransCode;            // Transaction code
    SHORT ErrorCode;            // Error code
    LONG BCSeqNo;               // Broadcast sequence number
    CHAR Reserved2[4];          // Reserved
    CHAR TimeStamp2[8];         // Time when message is sent
    CHAR Filler2[8];            // Machine number
    SHORT MessageLength;        // Message length
};
```

#### 3. SEC_INFO (12 bytes)
```c
struct SEC_INFO {
    CHAR Symbol[10];            // Security symbol
    CHAR Series[2];             // Security series
};
```

---

## Core Message Structures

### 1. Market By Order/Price Update (7200) Done
**Transaction Code**: BCAST_MBO_MBP_UPDATE (7200)
**Size**: 482 bytes
**Purpose**: Contains best buy/sell orders and market data

```c
struct BROADCAST_MBO_MBP {
    BCAST_HEADER header;                        // 40 bytes
    INTERACTIVE_MBO_DATA mboData;               // 240 bytes
    CHAR MBPBuffer[160];                        // MBP information for 10 levels
    SHORT BbTotalBuyFlag;                       // Buyback buy flag
    SHORT BbTotalSellFlag;                      // Buyback sell flag
    LONG LONG TotalBuyQuantity;                 // Total buy quantity
    LONG LONG TotalSellQuantity;                // Total sell quantity
    MBO_MBP_INDICATOR indicator;                // 2 bytes - Buy/Sell indicators
    LONG ClosingPrice;                          // Closing price
    LONG OpenPrice;                             // Opening price
    LONG HighPrice;                             // High price
    LONG LowPrice;                              // Low price
    CHAR Reserved[4];                           // Reserved
};

struct INTERACTIVE_MBO_DATA {
    LONG Token;                                 // Unique token for symbol-series
    SHORT BookType;                             // RL/ST/SL/OL/SP/AU
    SHORT TradingStatus;                        // 1=Preopen, 2=Open, 3=Suspended, etc.
    LONG LONG VolumeTradedToday;                // Total volume traded today
    LONG LastTradedPrice;                       // Last traded price
    CHAR NetChangeIndicator;                    // '+'/'-' for price change
    CHAR Filler;                                // Padding
    LONG NetPriceChangeFromClosingPrice;        // Net price change
    LONG LastTradeQuantity;                     // Last trade quantity
    LONG LastTradeTime;                         // Last trade time
    LONG AverageTradePrice;                     // Average trade price
    SHORT AuctionNumber;                        // Auction number (max 9999)
    SHORT AuctionStatus;                        // Auction status
    SHORT InitiatorType;                        // Control or trader
    LONG InitiatorPrice;                        // Initiator auction price
    LONG InitiatorQuantity;                     // Initiator auction quantity
    LONG AuctionPrice;                          // Auction price
    LONG AuctionQuantity;                       // Auction quantity
    CHAR MBOBuffer[180];                        // MBO information for 10 orders
};

struct MBO_INFORMATION {
    LONG TraderId;                              // Trader ID
    LONG Qty;                                   // Quantity
    LONG Price;                                 // Price
    ST_MBO_MBP_TERMS terms;                     // AON/MF flags
    LONG MinFillQty;                            // Minimum fill quantity
};

struct MBP_INFORMATION {
    LONG LONG Quantity;                         // Quantity at price level
    LONG Price;                                 // Price level
    SHORT NumberOfOrders;                       // Number of orders
    SHORT BbBuySellFlag;                        // Buyback/Market maker flags
};
```

### 2. Only Market By Price Update (7208) Done
**Transaction Code**: BCAST_ONLY_MBP (7208)
**Size**: 566 bytes
**Purpose**: Market by price data without order details

```c
struct BROADCAST_ONLY_MBP {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NoOfRecords;                          // Number of securities (max 2)
    INTERACTIVE_ONLY_MBP_DATA data[2];          // 262 bytes each
};

struct INTERACTIVE_ONLY_MBP_DATA {
    LONG Token;                                 // Token number
    SHORT BookType;                             // Book type
    SHORT TradingStatus;                        // Trading status
    LONG LONG VolumeTradedToday;                // Volume traded today
    LONG LastTradedPrice;                       // Last traded price
    CHAR NetChangeIndicator;                    // Price change indicator
    CHAR Filler;                                // Padding
    LONG NetPriceChangeFromClosingPrice;        // Net price change
    LONG LastTradeQuantity;                     // Last trade quantity
    LONG LastTradeTime;                         // Last trade time
    LONG AverageTradePrice;                     // Average trade price
    SHORT AuctionNumber;                        // Auction number
    SHORT AuctionStatus;                        // Auction status
    SHORT InitiatorType;                        // Initiator type
    LONG InitiatorPrice;                        // Initiator price
    LONG InitiatorQuantity;                     // Initiator quantity
    LONG AuctionPrice;                          // Auction price
    LONG AuctionQuantity;                       // Auction quantity
    CHAR RecordBuffer[160];                     // MBP information (10 levels)
    SHORT BbTotalBuyFlag;                       // Buyback buy flag
    SHORT BbTotalSellFlag;                      // Buyback sell flag
    LONG LONG TotalBuyQuantity;                 // Total buy quantity
    LONG LONG TotalSellQuantity;                // Total sell quantity
    MBP_INDICATOR indicator;                    // Buy/sell indicators
    LONG ClosingPrice;                          // Closing price
    LONG OpenPrice;                             // Opening price
    LONG HighPrice;                             // High price
    LONG LowPrice;                              // Low price
    LONG IndicativeClosePrice;                  // Indicative close price
};
```

### 3. Market Watch Update (7201) Done
**Transaction Code**: BCAST_MW_ROUND_ROBIN (7201)
**Size**: 466 bytes
**Purpose**: Market watch information with best buy/sell and last trade

```c
struct BROADCAST_INQUIRY_RESPONSE {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of records (max 4)
    MARKETWATCHBROADCAST data[4];               // 106 bytes each
};

struct MARKETWATCHBROADCAST {
    LONG Token;                                 // Token number
    MARKET_WISE_INFORMATION marketInfo[3];      // 34 bytes each for 3 markets
};

struct MARKET_WISE_INFORMATION {
    MBO_MBP_INDICATOR indicator;                // Buy/sell indicators
    LONG LONG BuyVolume;                        // Best buy volume
    LONG BuyPrice;                              // Best buy price
    LONG LONG SellVolume;                       // Best sell volume
    LONG SellPrice;                             // Best sell price
    LONG LastTradePrice;                        // Last trade price
    LONG LastTradeTime;                         // Last trade time
};
```

### 4. Ticker and Market Index (18703) Done
**Transaction Code**: BCAST_TICKER_AND_MKT_INDEX (18703)
**Size**: 546 bytes
**Purpose**: Ticker and market index information

```c
struct TICKER_TRADE_DATA {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of records (max 28)
    TICKER_INDEX_INFORMATION records[28];       // 18 bytes each
};

struct TICKER_INDEX_INFORMATION {
    LONG Token;                                 // Token number
    SHORT MarketType;                           // Market type
    LONG FillPrice;                             // Trade price
    LONG FillVolume;                            // Trade volume
    LONG MarketIndexValue;                      // Market index value
};
```

### 5. Multiple Index Broadcast (7207) 
**Transaction Code**: BCAST_INDICES (7207)
**Size**: 474 bytes
**Purpose**: Multiple index broadcast data

```c
struct BROADCAST_INDICES {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of indices (max 6)
    INDICES indices[6];                         // 71 bytes each
};

struct INDICES {
    CHAR IndexName[21];                         // Index name (e.g., "Nifty")
    LONG IndexValue;                            // Current index value
    LONG HighIndexValue;                        // Day's high index value
    LONG LowIndexValue;                         // Day's low index value
    LONG OpeningIndex;                          // Opening index value
    LONG ClosingIndex;                          // Closing index value
    LONG PercentChange;                         // Percent change
    LONG YearlyHigh;                            // Yearly high
    LONG YearlyLow;                             // Yearly low
    LONG NoOfUpmoves;                           // Number of up moves
    LONG NoOfDownmoves;                         // Number of down moves
    DOUBLE MarketCapitalisation;                // Market cap
    CHAR NetChangeIndicator;                    // '+'/'-'/' ' for change
    CHAR FILLER;                                // Padding
};
```

### 6. India VIX Index Broadcast (7216)
**Transaction Code**: BCAST_INDICES_VIX (7216)
**Size**: 474 bytes
**Purpose**: India VIX index broadcast

```c
struct BROADCAST_INDICES_VIX {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of records
    INDICES indices[6];                         // Same structure as regular indices
};
```

### 7. Indicative Index Broadcast (8207)
**Transaction Code**: BCAST_INDICATIVE_INDICES (8207)
**Size**: 474 bytes
**Purpose**: Indicative index broadcast (30 minutes before market close)

```c
struct BROADCAST_INDICATIVE_INDICES {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of indices
    INDICATIVE_INDICES indices[6];              // 71 bytes each
};

struct INDICATIVE_INDICES {
    CHAR IndexName[21];                         // Index name
    LONG IndicativeCloseValue;                  // Indicative close value
    LONG Reserved1[4];                          // Reserved fields
    LONG ClosingIndex;                          // Previous close
    LONG PercentChange;                         // Percent change
    LONG Reserved2[2];                          // Reserved
    LONG Change;                                // Absolute change
    LONG Reserved3;                             // Reserved
    DOUBLE MarketCapitalization;                // Market cap
    CHAR NetChangeIndicator;                    // Change indicator
    CHAR FILLER;                                // Padding
};
```

### 8. Security Master Change (18720)
**Transaction Code**: BCAST_SECURITY_MSTR_CHG (18720)
**Size**: 260 bytes
**Purpose**: Security master data changes

```c
struct SECURITY_UPDATE_INFORMATION {
    BCAST_HEADER header;                        // 40 bytes
    LONG Token;                                 // Token number
    CHAR Symbol[10];                            // Symbol
    CHAR Series[2];                             // Series
    CHAR SecurityName[25];                      // Security name
    LONG ExpDate;                               // Expiry date
    LONG StrikePrice;                           // Strike price
    CHAR OptionType[2];                         // CE/PE for options
    CHAR AssetToken[10];                        // Asset token
    LONG IssueRate;                             // Issue rate
    LONG IssuedCapital;                         // Issued capital
    LONG ListingDate;                           // Listing date
    LONG MaturityDate;                          // Maturity date
    LONG MarketLot;                             // Market lot
    CHAR IsinNumber[12];                        // ISIN number
    LONG BoardLotQuantity;                      // Board lot quantity
    LONG CorporateAction;                       // Corporate action
    LONG CorporateActionPrice;                  // CA price
    LONG FreezeQtyUpperRange;                   // Upper freeze qty
    LONG FreezeQtyLowerRange;                   // Lower freeze qty
    LONG DynamicQtyUpperRange;                  // Dynamic upper qty
    LONG DynamicQtyLowerRange;                  // Dynamic lower qty
    LONG CreditRating;                          // Credit rating
    CHAR SecurityStatus;                        // Security status
    CHAR BookClosureStartDate[8];               // Book closure start
    CHAR BookClosureEndDate[8];                 // Book closure end
    CHAR InstrumentName[6];                     // Instrument name
    LONG MaxSingleTransactionQty;               // Max single transaction qty
    LONG IssueStartDate;                        // Issue start date
    LONG InterestPaymentDate;                   // Interest payment date
    LONG IssueMaturityDate;                     // Issue maturity date
    LONG MarginPercentage;                      // Margin percentage
    LONG MinimumLotQuantity;                    // Minimum lot quantity
    CHAR Reserved[83];                          // Reserved space
};
```

### 9. Security Status Change (18130/18707)
**Transaction Code**: BCAST_SECURITY_STATUS_CHG (18130) / BCAST_SECURITY_STATUS_CHG_PREOPEN (18707)
**Size**: 442 bytes
**Purpose**: Security status updates

```c
struct SECURITY_STATUS_UPDATE_INFORMATION {
    BCAST_HEADER header;                        // 40 bytes
    LONG Token;                                 // Token number
    CHAR Symbol[10];                            // Symbol
    CHAR Series[2];                             // Series
    CHAR SecurityStatus;                        // Security status
    CHAR TradingStatus;                         // Trading status
    LONG SuspendedToPrice;                      // Suspended to price
    LONG SuspendedToTime;                       // Suspended to time
    CHAR MarketType;                            // Market type
    CHAR BookType;                              // Book type
    CHAR ActionCode;                            // Action code
    CHAR PercentChange;                         // Percent change allowed
    CHAR DailyPriceRangePercentage;             // Daily price range %
    CHAR CircuitFilters;                        // Circuit filters
    CHAR AuctionFlag;                           // Auction flag
    CHAR ExtendedHoursFlag;                     // Extended hours flag
    CHAR ILSFlag;                               // ILS flag
    CHAR BlackoutPeriodFlag;                    // Blackout period flag
    CHAR FreeFloatMarketCapFlag;                // Free float market cap flag
    CHAR SettlementType;                        // Settlement type
    CHAR BookClosureFlag;                       // Book closure flag
    CHAR RightsFlag;                            // Rights flag
    CHAR BoardMeetingFlag;                      // Board meeting flag
    CHAR InterestFlag;                          // Interest flag
    CHAR DividendFlag;                          // Dividend flag
    CHAR BonusFlag;                             // Bonus flag
    CHAR CallPutOption;                         // Call/Put option
    CHAR CashFlag;                              // Cash flag
    CHAR RegularFlag;                           // Regular flag
    CHAR ExerciseFlag;                          // Exercise flag
    CHAR OddLotFlag;                            // Odd lot flag
    CHAR AuctionAllowedFlag;                    // Auction allowed flag
    CHAR SLMFlag;                               // SLM flag
    CHAR TbtFlag;                               // TBT flag
    LONG FreezeQtyLowerRange;                   // Freeze qty lower range
    LONG FreezeQtyUpperRange;                   // Freeze qty upper range
    LONG DynamicQtyUpperRange;                  // Dynamic qty upper range
    LONG DynamicQtyLowerRange;                  // Dynamic qty lower range
    LONG OldToken;                              // Old token
    CHAR Reserved[332];                         // Reserved
};
```

### 10. System Information (7206)
**Transaction Code**: BCAST_SYSTEM_INFORMATION_OUT (7206)
**Size**: 90 bytes
**Purpose**: System information broadcast

```c
struct SYSTEM_INFORMATION_DATA {
    BCAST_HEADER header;                        // 40 bytes
    SHORT MarketType;                           // Market type
    CHAR MarketStatus;                          // Market status
    CHAR Reserved1;                             // Reserved
    LONG MarketStatusTime;                      // Market status time
    CHAR ExchangeMessage[40];                   // Exchange message
    CHAR Reserved2[4];                          // Reserved
};
```

### 11. General Broadcast Messages (6501 and related)
**Transaction Codes**: 
- BCAST_JRNL_VCT_MSG (6501)
- BC_OPEN_MESSAGE (6511)
- BC_CLOSE_MESSAGE (6521)
- BC_PREOPEN_SHUTDOWN_MSG (6531)
- BC_NORMAL_MKT_PREOPEN_ENDED (6571)
- BC_CLOSING_START (6583)
- BC_CLOSING_END (6584)

**Size**: 298 bytes
**Purpose**: Various market status and control messages

```c
struct BCAST_VCT_MESSAGES {
    BCAST_HEADER header;                        // 40 bytes
    CHAR VctMessage[258];                       // VCT message content
};
```

### 12. Auction Activity Message (18700)
**Transaction Code**: BCAST_AUCTION_INQUIRY_OUT (18700)
**Size**: 76 bytes
**Purpose**: Auction activity information

```c
struct MS_AUCTION_INQ_DATA {
    BCAST_HEADER header;                        // 40 bytes
    LONG Token;                                 // Token number
    SHORT AuctionNumber;                        // Auction number
    SHORT AuctionStatus;                        // Auction status
    SHORT InitiatorType;                        // Initiator type
    LONG TotalBuy;                              // Total buy quantity
    LONG BestBuyPrice;                          // Best buy price
    LONG TotalSell;                             // Total sell quantity
    LONG BestSellPrice;                         // Best sell price
    LONG AuctionPrice;                          // Auction price
    LONG AuctionQuantity;                       // Auction quantity
    SHORT SettlementPeriod;                     // Settlement period
};
```

### 13. Auction Status Change (6581)
**Transaction Code**: BC_AUCTION_STATUS_CHANGE (6581)
**Size**: 302 bytes
**Purpose**: Auction status change notifications

```c
struct AUCTION_STATUS_CHANGE {
    BCAST_HEADER header;                        // 40 bytes
    LONG Token;                                 // Token number
    SHORT AuctionNumber;                        // Auction number
    CHAR AuctionStatus;                         // New auction status
    CHAR Reserved[259];                         // Reserved space
};
```

### 14. Call Auction Market Watch (7215)
**Transaction Code**: BCAST_CA_MW (7215)
**Size**: 482 bytes
**Purpose**: Call auction market watch data

```c
struct BROADCAST_CALL_AUCTION_MARKET_WATCH {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NoOfRecords;                          // Number of records (max 11)
    MARKETWATCHBROADCAST_CA data[11];           // 40 bytes each
};

struct MARKETWATCHBROADCAST_CA {
    LONG Token;                                 // Token number
    SHORT MktType;                              // Market type (5=CA1, 6=CA2)
    MBO_MBP_INDICATOR indicator;                // Buy/sell indicators
    LONG LONG BuyVolume;                        // Buy volume
    LONG BuyPrice;                              // Buy price
    LONG LONG SellVolume;                       // Sell volume
    LONG SellPrice;                             // Sell price
    LONG LastTradePrice;                        // Last trade price
    LONG LastTradeTime;                         // Last trade time
};
```

### 15. Call Auction Order Cancel Update (7210)
**Transaction Code**: BCAST_CALL_AUCTION_ORD_CXL_UPDATE (7210)
**Size**: 490 bytes
**Purpose**: Order cancellation statistics during call auction

```c
struct BROADCAST_CALL_AUCTION_ORD_CXL_UPDATE {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NoOfRecords;                          // Number of records (max 8)
    INTERACTIVE_ORD_CXL_DETAILS details[8];     // 56 bytes each
};

struct INTERACTIVE_ORD_CXL_DETAILS {
    LONG Token;                                 // Token number
    CHAR Filler[4];                             // Filler
    LONG LONG BuyOrdCxlCount;                   // Buy order cancel count
    LONG LONG BuyOrdCxlVol;                     // Buy order cancel volume
    LONG LONG SellOrdCxlCount;                  // Sell order cancel count
    LONG LONG SellOrdCxlVol;                    // Sell order cancel volume
    CHAR Reserved[16];                          // Reserved
};
```

### 16. Buy Back Information (18708)
**Transaction Code**: BCAST_BUY_BACK (18708)
**Size**: 426 bytes
**Purpose**: Buy back information broadcast

```c
struct BROADCAST_BUY_BACK {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of records (max 6)
    BUYBACKDATA data[6];                        // 64 bytes each
};

struct BUYBACKDATA {
    LONG Token;                                 // Token number
    CHAR Symbol[10];                            // Symbol
    CHAR Series[2];                             // Series
    DOUBLE PdayCumVol;                          // Previous day cumulative volume
    LONG PdayHighPrice;                         // Previous day high price
    LONG PdayLowPrice;                          // Previous day low price
    LONG PdayWtAvg;                             // Previous day weighted average
    DOUBLE CdayCumVol;                          // Current day cumulative volume
    LONG CdayHighPrice;                         // Current day high price
    LONG CdayLowPrice;                          // Current day low price
    LONG CdayWtAvg;                             // Current day weighted average
    LONG StartDate;                             // Buy back start date
    LONG EndDate;                               // Buy back end date
};
```

### 17. Turnover/Broker Status Messages (9010/9011)
**Transaction Codes**: 
- BCAST_TURNOVER_EXCEEDED (9010)
- BROADCAST_BROKER_REACTIVATED (9011)

**Size**: 77 bytes
**Purpose**: Broker turnover limit and reactivation messages

```c
struct BROADCAST_LIMIT_EXCEEDED {
    BCAST_HEADER header;                        // 40 bytes
    CHAR BrokerNumber[5];                       // Broker number
    CHAR ParticipantName[25];                   // Participant name
    CHAR ReasonCode;                            // Reason code
    CHAR Reserved[6];                           // Reserved
};
```

### 18. Call Auction MBP Broadcast (7214)
**Transaction Code**: BCAST_CALL_AUCTION_MBP (7214)
**Size**: 538 bytes
**Purpose**: Call auction market by price broadcast during preopen session

```c
struct BROADCAST_CALL_AUCTION_MBP {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NoOfRecords;                          // Number of securities (max 2)
    INTERACTIVE_CALL_AUCTION_MBP_DATA data[2];  // 248 bytes each
};

struct INTERACTIVE_CALL_AUCTION_MBP_DATA {
    LONG Token;                                 // Token number
    SHORT BookType;                             // CA=11 (Call Auction1), CB=12 (Call Auction2)
    SHORT TradingStatus;                        // 1=Preopen, 2=Open, 6=Price Discovery
    LONG LONG VolumeTradedToday;                // Volume traded today
    LONG LONG IndicativeTradedQty;              // Indicative traded quantity
    LONG LastTradedPrice;                       // Last traded price
    CHAR NetChangeIndicator;                    // '+'/'-' for price change
    CHAR Filler;                                // Padding
    LONG NetPriceChangeFromClosingPrice;        // Net price change
    LONG LastTradeQuantity;                     // Last trade quantity
    LONG LastTradeTime;                         // Last trade time
    LONG AverageTradePrice;                     // Average trade price
    LONG FirstOpenPrice;                        // First trade open price for call auction
    CHAR RecordBuffer[160];                     // MBP information (10 levels)
    SHORT BbTotalBuyFlag;                       // Buyback buy flag
    SHORT BbTotalSellFlag;                      // Buyback sell flag
    LONG LONG TotalBuyQuantity;                 // Total buy quantity
    LONG LONG TotalSellQuantity;                // Total sell quantity
    MBP_INDICATOR indicator;                    // Buy/sell indicators
    LONG ClosingPrice;                          // Closing price
    LONG OpenPrice;                             // Opening/Indicative opening price
    LONG HighPrice;                             // High price
    LONG LowPrice;                              // Low price
};
```

### 19. Participant Master Change (7306)
**Transaction Code**: BCAST_PART_MSTR_CHG (7306)
**Size**: 84 bytes
**Purpose**: Participant master data changes

```c
struct PARTICIPANT_UPDATE_INFO {
    BCAST_HEADER header;                        // 40 bytes
    CHAR ParticipantId[12];                     // Participant ID
    CHAR ParticipantName[25];                   // Participant name
    CHAR ParticipantStatus;                     // 'S'=Suspended, 'A'=Active
    LONG ParticipantUpdateDateTime;             // Time of change (seconds from Jan 1, 1980)
    CHAR DeleteFlag;                            // Delete flag
    CHAR Reserved;                              // Reserved
};
```

### 20. Security Bhav Copy (18201)
**Transaction Code**: MARKET_STATS_REPORT_DATA (18201)
**Sizes**: Header=106 bytes, Data=478 bytes, Trailer=46 bytes
**Purpose**: End-of-day security market statistics

```c
// Header Structure
struct MS_RP_HDR {
    MESSAGE_HEADER header;                      // 40 bytes
    CHAR MsgType;                               // 'H' for header
    LONG ReportDate;                            // Report date
    SHORT UserType;                             // User type (-1)
    CHAR BrokerId[5];                           // Trading member ID (blank)
    CHAR BrokerName[25];                        // Broker name (blank)
    SHORT TraderNumber;                         // Trader ID (zero)
    CHAR TraderName[26];                        // Trader name (blank)
};

// Data Structure
struct REPORT_MARKET_STATISTICS {
    MESSAGE_HEADER header;                      // 40 bytes
    CHAR MessageType;                           // Message type
    CHAR Reserved;                              // Reserved
    SHORT NumberOfRecords;                      // Number of records
    MARKET_STATISTICS_DATA data[7];             // 62 bytes each (max 7 records)
};

struct MARKET_STATISTICS_DATA {
    SEC_INFO secInfo;                           // 12 bytes - Symbol and series
    SHORT MarketType;                           // Market type
    LONG OpenPrice;                             // Opening price
    LONG HighPrice;                             // High price
    LONG LowPrice;                              // Low price
    LONG ClosingPrice;                          // Closing price
    LONG LONG TotalQuantityTraded;              // Total quantity traded
    DOUBLE TotalValueTraded;                    // Total value traded
    LONG PreviousClosePrice;                    // Previous close price
    LONG FiftyTwoWeekHigh;                      // 52-week high
    LONG FiftyTwoWeekLow;                       // 52-week low
    CHAR CorporateActionIndicator[4];           // Corporate action indicator
};

// Trailer Structure
struct REPORT_TRAILER {
    MESSAGE_HEADER header;                      // 40 bytes
    CHAR MessageType;                           // 'T' for trailer
    CHAR Reserved[5];                           // Reserved
};
```

### 21. Index Bhav Copy (1836)
**Transaction Code**: MKT_IDX_RPT_DATA (1836)
**Size**: 464 bytes
**Purpose**: End-of-day index market statistics

```c
struct MS_RP_MARKET_INDEX {
    MESSAGE_HEADER header;                      // 40 bytes
    CHAR MsgType;                               // 'R' for report
    CHAR Reserved;                              // Reserved
    SHORT NoOfIndexRecs;                        // Number of index records
    MKT_INDEX indices[7];                       // 60 bytes each (max 7 indices)
};

struct MKT_INDEX {
    CHAR IndName[24];                           // Index name (e.g., "CNX")
    LONG MktIndexPrevClose;                     // Previous close value
    LONG MktIndexOpening;                       // Opening index value
    LONG MktIndexHigh;                          // High index value
    LONG MktIndexLow;                           // Low index value
    LONG MktIndexClosing;                       // Closing index value
    LONG MktIndexPercent;                       // Percentage change
    LONG MktIndexYrHi;                          // Yearly high
    LONG MktIndexYrLo;                          // Yearly low
    LONG MktIndexStart;                         // Starting value
};
```

### 22. Industry Indices Broadcast (7203)
**Transaction Code**: BCAST_IND_INDICES (7203)
**Size**: 484 bytes
**Purpose**: Industry index values broadcast

```c
struct BROADCAST_INDUSTRY_INDICES {
    BCAST_HEADER header;                        // 40 bytes
    SHORT NumberOfRecords;                      // Number of indices (max 17)
    INDUSTRY_INDICES indices[17];               // 25 bytes each
};

struct INDUSTRY_INDICES {
    CHAR IndustryName[21];                      // Industry name (e.g., "Defty", "CNX IT")
    LONG IndexValue;                            // Current index value
};
```

### 23. Security-Level Status Change Action (7764)
**Transaction Code**: BC_SYMBOL_STATUS_CHANGE_ACTION (7764)
**Size**: 58 bytes
**Purpose**: Security-level trading/market status change messages

```c
struct BCAST_SYMBOL_STATUS_CHANGE_ACTION {
    BCAST_HEADER header;                        // 40 bytes
    SEC_INFO secInfo;                           // 12 bytes - Symbol and series
    SHORT MarketType;                           // Market type (1=Normal, 2=Odd Lot, etc.)
    SHORT Reserved;                             // Reserved
    SHORT ActionCode;                           // Action code (6531, 6571, 6511, 6521, 6583, 6584)
};
```

---

## Important Processing Notes

### 1. Byte Order (Endianness)
- NSE uses **big-endian** byte order
- If your system is little-endian, you must **twiddle** (byte reverse) multi-byte fields
- Apply to: UINT, SHORT, LONG, DOUBLE, LONG LONG

### 2. Price Processing
- All price fields are in **paisa** (multiply by 100 before sending, divide by 100 after receiving)
- Value limits are also multiplied by (100000 * 100)

### 3. Text Processing
- Convert all alphabetical data to **uppercase** before sending (except passwords)
- No NULL-terminated strings - use **blank padding**
- Received strings are blank-padded, not NULL-terminated

### 4. Structure Alignment
- Use **pragma pack 2**
- Items of type char/unsigned char are byte aligned
- All other structure members are word aligned
- Pad odd-sized structures to even number of bytes

### 5. Broadcast Packet Format
- First byte indicates market type (ignore next 7 bytes)
- If first byte = 2, it's Futures & Options market
- Message header starts from 9th byte
- **LZO compression** is used for broadcast data

### 6. Time Fields
- All time fields are seconds from midnight January 1, 1980
- TimeStamp field contains nanoseconds for specific transaction codes
- TimeStamp1 contains time in jiffies (split into two 4-byte doubles)

---

## Message Processing Workflow

### For Real-Time Feed Handler:

1. **Connect to broadcast circuit** (BCID - unidirectional from NSE to client)
2. **Handle LZO decompression** of incoming packets
3. **Parse message headers** to identify transaction codes
4. **Route messages** to appropriate handlers based on transaction code
5. **Apply byte order corrections** if on little-endian system
6. **Extract and normalize price data** (divide by 100)
7. **Update internal data structures** with market data

### Priority Processing Order:
1. **System Information** (7206) - Market status updates
2. **Security Master/Status Changes** (18720, 18130/18707, 7306) - Security/participant metadata
3. **Market Data Updates** (7200, 7208, 7201) - Core order book data
4. **Call Auction Data** (7214, 7215, 7210) - Call auction specific data
5. **Index Data** (7207, 7216, 8207, 7203) - Index updates
6. **Ticker Data** (18703) - Trade confirmations
7. **Control Messages** (6501, 6511-6584, 7764) - Market state changes
8. **Bhav Copy Data** (18201, 1836) - End-of-day reports
9. **Auction/Turnover Messages** (18700, 6581, 9010/9011) - Auction activity and broker status

### Message Categorization for Processing:

#### High-Frequency Real-Time Data (Process Immediately):
- **7200, 7208, 7201** - Market data updates
- **18703** - Ticker updates
- **7207, 7216, 8207** - Index updates

#### Medium-Frequency Control Data (Process with Priority):
- **7206** - System information
- **18720, 18130, 18707** - Security status changes
- **7214, 7215, 7210** - Call auction data
- **6501, 6511-6584** - Market status messages

#### Low-Frequency Administrative Data (Process in Background):
- **7306** - Participant changes
- **18708** - Buy back information
- **18201, 1836** - Bhav copy data
- **7203** - Industry indices
- **7764** - Security-level status changes
- **18700, 6581** - Auction activity
- **9010, 9011** - Turnover/broker status

---

## Error Handling

### Common Error Codes:
- **ERR_INVALID_MSG_LENGTH**: Invalid message length
- **ERR_CHECKSUM_FAILED**: Checksum verification failed
- **ERR_ENCRYPTION_FLAG_MISMATCH**: Encryption flag mismatch

### Message Validation:
- Verify message length matches expected structure size
- Check transaction code is valid and expected
- Validate token numbers exist in security master
- Ensure timestamps are reasonable

---

## Implementation Notes

### Message Handler Architecture

1. **Buffer Management**: Pre-allocate buffers for each message type based on packet sizes
2. **Threading Strategy**: 
   - **Network I/O Thread**: Handle broadcast circuit reception and LZO decompression
   - **High-Frequency Processing Thread**: Process market data messages (7200, 7208, 7201, 18703)
   - **Index Processing Thread**: Handle index updates (7207, 7216, 8207, 7203)
   - **Control Message Thread**: Process system and status messages
   - **Background Processing Thread**: Handle administrative messages and bhav copy data

3. **Queuing Strategy**: 
   - **Lock-free queues** between network and processing threads
   - **Priority queues** for different message categories
   - **Separate queues** for high-frequency vs. low-frequency messages

4. **Data Management**:
   - **Security Master Cache**: Maintain token-to-symbol mappings from 18720 messages
   - **Market Status Cache**: Track market states from 7206 and 6501-6584 messages
   - **Participant Cache**: Store participant information from 7306 messages
   - **Order Book Snapshots**: Maintain current state from MBP/MBO updates

### Performance Optimizations

1. **Message Filtering**: Filter messages by token or symbol to process only relevant securities
2. **Batch Processing**: Group similar message types for efficient processing
3. **Memory Pools**: Pre-allocate memory for message structures to avoid allocation overhead
4. **CPU Affinity**: Pin processing threads to specific CPU cores
5. **NUMA Awareness**: Allocate memory on correct NUMA nodes for multi-socket systems

### Monitoring and Diagnostics

1. **Message Rate Tracking**: Monitor messages per second for each transaction code
2. **Processing Latency**: Measure time from packet receipt to data update
3. **Queue Depth Monitoring**: Track queue sizes to identify bottlenecks
4. **Error Rate Tracking**: Monitor parse errors and invalid messages
5. **Sequence Number Gaps**: Detect missing broadcast packets
6. **Market State Validation**: Ensure market status consistency across messages

### Failover and Recovery

1. **Graceful Degradation**: Continue processing available messages if some fail
2. **Circuit Reconnection**: Handle broadcast circuit disconnections automatically
3. **State Recovery**: Rebuild market state from snapshot messages after reconnection
4. **Message Replay**: Request missed messages if sequence gaps are detected
5. **Backup Circuits**: Support multiple broadcast circuits for redundancy

This comprehensive documentation covers all the essential NSE Capital Market message codes and structures needed for building a robust real-time feed handler that can handle the complete spectrum of market data, from high-frequency trading updates to administrative control messages.
