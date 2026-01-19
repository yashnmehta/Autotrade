#ifndef UNIFIED_PRICE_STATE_H
#define UNIFIED_PRICE_STATE_H

#include <cstdint>
#include <cstring>

namespace MarketData {

/**
 * @brief Market depth level information
 */
struct DepthLevel {
    double price = 0.0;
    uint32_t quantity = 0;
    uint32_t orders = 0;

    DepthLevel() = default;
    DepthLevel(double p, uint32_t q, uint32_t o) : price(p), quantity(q), orders(o) {}
};

/**
 * @brief Unified record combining all market data fields for any token.
 * This is the "Single Source of Truth" for an instrument across the application.
 * 
 * Architecture:
 * - Fused State: Combines Touchline, Depth, OI, LPP, etc.
 * - Multi-Segment: Can represent NSE FO, NSE CM, or BSE instruments.
 * - Raw/Natural Units: Prices in Doubles (Rupees), Volume in 64-bit Ints.
 */
struct UnifiedState {
    // =========================================================
    // 1. IDENTIFICATION & METADATA
    // =========================================================
    uint32_t token = 0;
    uint16_t exchangeSegment = 0; // 1=NSECM, 2=NSEFO, 3=BSECM, 4=BSEFO (Unified numbering)

    // Contract Master Info (Static)
    char symbol[32] = {0};
    char displayName[64] = {0};
    char series[16] = {0};
    char scripCode[16] = {0};       // BSE Scrip Code
    int32_t lotSize = 0;
    double tickSize = 0.0;
    double strikePrice = 0.0;
    char optionType[3] = {0};       // CE/PE/XX
    char expiryDate[16] = {0};      // DDMMMYYYY
    int64_t assetToken = 0;
    int32_t instrumentType = 0;     // 1=Future, 2=Option, 3=Equities, etc.
    
    // =========================================================
    // 2. DYNAMIC MARKET DATA (LTP, OHLC, Volume)
    // =========================================================
    double ltp = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double avgPrice = 0.0;
    
    uint64_t volume = 0;             // Cumulative volume traded today
    uint64_t turnover = 0;           // Cumulative turnover / value
    uint32_t lastTradeQty = 0;

    uint32_t lastTradeTime = 0;      // Seconds since midnight or epoch
    
    char netChangeIndicator = ' ';   // '+' or '-'
    double netChange = 0.0;
    double percentChange = 0.0;
    
    // =========================================================
    // 3. MARKET DEPTH (5 Levels)
    // =========================================================
    DepthLevel bids[5];
    DepthLevel asks[5];
    uint64_t totalBuyQty = 0;
    uint64_t totalSellQty = 0;
    
    // =========================================================
    // 4. DERIVATIVES SPECIFIC (OI, IV)
    // =========================================================
    int64_t openInterest = 0;
    int64_t openInterestChange = 0;
    double impliedVolatility = 0.0;
    
    // =========================================================
    // 5. STATUS & LIMITS
    // =========================================================
    uint16_t tradingStatus = 0;      // 1=Preopen, 2=Open, 3=Suspended, etc.
    uint16_t bookType = 0;
    double lowerCircuit = 0.0;
    double upperCircuit = 0.0;
    
    // =========================================================
    // 6. DIAGNOSTICS
    // =========================================================
    int64_t lastPacketTimestamp = 0; // Nanoseconds since epoch
    uint32_t updateCount = 0;        // Number of updates received
    bool isUpdated = false;          // True if any dynamic field changed since last reset

    UnifiedState() {
        std::memset(bids, 0, sizeof(bids));
        std::memset(asks, 0, sizeof(asks));
    }
};

} // namespace MarketData

#endif // UNIFIED_PRICE_STATE_H
