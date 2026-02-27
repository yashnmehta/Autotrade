#ifndef UDP_ENUMS_H
#define UDP_ENUMS_H

#include <cstdint>
#include "core/ExchangeSegment.h"

namespace UDP {

/**
 * @brief DEPRECATED — Use ::ExchangeSegment from "core/ExchangeSegment.h" directly.
 *
 * This alias exists for backward compatibility so that existing code using
 * UDP::ExchangeSegment still compiles without changes.  New code should
 * use the global ::ExchangeSegment enum.
 */
using ExchangeSegment = ::ExchangeSegment;

/**
 * @brief NSE broadcast message types (Transaction Codes)
 * 
 * From NSE TRIMM Protocol v9.46 - Chapter 9: Broadcast Messages
 */
enum class NSEMessageType : uint16_t {
    // === CRITICAL: Real-time Price Updates ===
    BCAST_MBO_MBP_UPDATE = 7200,              // Touchline + 5-level depth
    BCAST_ONLY_MBP = 7208,                    // Market By Price (2 records)
    
    // === HIGH: Trade & OI Updates ===
    BCAST_TICKER_AND_MKT_INDEX = 7202,        // Ticker with OI changes (17 records)
    BCAST_MW_ROUND_ROBIN = 7201,              // Market Watch (3 market types)
    
    // === NORMAL: Index Updates ===
    BCAST_INDICES = 7207,                     // 6 major indices (NIFTY, etc.)
    BCAST_INDUSTRY_INDEX_UPDATE = 7203,       // 20 industry indices
    BCAST_GLOBAL_INDICES = 7732,              // Global indices
    
    // === HIGH: Circuit Limits & Protection ===
    BCAST_LIMIT_PRICE_PROTECTION = 7220,      // Price protection range
    
    // === NORMAL: Spread Trading ===
    BCAST_SPD_MBP_DELTA = 7211,               // Spread market depth delta
    
    // === LOW: Security Master Updates ===
    BCAST_SECURITY_MSTR_CHG = 7305,           // Security master change
    BCAST_SEC_MSTR_CHNG_PERIODIC = 7340,      // Periodic master update
    
    // === ENHANCED: 64-bit Support ===
    BCAST_ENHNCD_MW_ROUND_ROBIN = 17201,      // Enhanced Market Watch (64-bit OI)
    BCAST_ENHNCD_TICKER = 17202,              // Enhanced Ticker (64-bit OI)
    ENHNCD_MKT_MVMT_CM_OI_IN = 17130,         // Enhanced CM Asset OI
    
    // === LOW: Statistics & Reports ===
    MKT_MVMT_CM_OI_IN = 7130,                 // CM Asset OI (58 records)
    RPRT_MARKET_STATS_OUT_RPT = 1833,         // Market statistics report
    ENHNCD_RPRT_MARKET_STATS = 11833,         // Enhanced market stats
    
    // === NSE CM Specific ===
    CM_TICKER = 18703                          // CM-specific ticker with market index
};

/**
 * @brief BSE broadcast message types
 * 
 * From BSE Direct NFCAST Protocol v5.0
 */
enum class BSEMessageType : uint16_t {
    // === CRITICAL: Real-time Price Updates ===
    MARKET_PICTURE = 2020,                    // LTP, OHLC, 5-level depth (32-bit token)
    MARKET_PICTURE_COMPLEX = 2021,            // Same as 2020 (64-bit token support)
    OPEN_INTEREST = 2015,                     // OI for derivatives
    
    // === HIGH: Index & Close Price ===
    INDEX_CHANGE = 2012,                      // Index with OHLC
    INDEX_CHANGE_SIMPLE = 2011,               // Index values only
    CLOSE_PRICE = 2014,                       // Close price broadcast
    
    // === NORMAL: Market State ===
    PRODUCT_STATE_CHANGE = 2002,              // Session state (pre-open, continuous, auction)
    AUCTION_SESSION_CHANGE = 2003,            // Shortage auction state
    
    // === NORMAL: Risk & Margins ===
    VAR_PERCENTAGE = 2016,                    // Margin percentage
    LIMIT_PRICE_PROTECTION = 2034,            // Circuit limits (DPR)
    
    // === NORMAL: Options ===
    IMPLIED_VOLATILITY = 2028,                // IV for options
    
    // === LOW: Auction & Odd Lots ===
    AUCTION_MARKET_PICTURE = 2017,            // Auction order book (sell-side)
    ODD_LOT_MARKET_PICTURE = 2027,            // Odd-lot order book
    CALL_AUCTION_CANCELLED_QTY = 2035,        // Cancelled quantity in auction
    
    // === LOW: Debt & FX ===
    DEBT_MARKET_PICTURE = 2033,               // Debt instrument data
    RBI_REFERENCE_RATE = 2022,                // USD reference rate
    
    // === LOW: System Messages ===
    TIME_BROADCAST = 2001,                    // Time sync (Hour/Min/Sec/Msec)
    AUCTION_KEEP_ALIVE = 2030,                // Network keep-alive
    NEWS_HEADLINE = 2004                      // News URL
};

/**
 * @brief Market session state
 */
enum class SessionState : uint8_t {
    PRE_OPEN = 0,       // Pre-open session
    CONTINUOUS = 1,     // Normal continuous trading
    AUCTION = 2,        // Call auction
    CLOSED = 3,         // Market closed
    POST_CLOSE = 4,     // Post-closing session
    UNKNOWN = 255       // Unknown/Invalid state
};

/**
 * @brief Message processing priority
 */
enum class MessagePriority : uint8_t {
    CRITICAL = 0,  // Price updates (7200, 7208, 2020, 2021) - Process immediately
    HIGH = 1,      // OI updates, indices (7202, 2015, 7207) - Process quickly
    NORMAL = 2,    // Market watch, statistics - Normal queue
    LOW = 3        // Master updates, news - Background processing
};

/**
 * @brief Get message priority based on type
 */
inline MessagePriority getMessagePriority(NSEMessageType msgType) {
    switch (msgType) {
        case NSEMessageType::BCAST_MBO_MBP_UPDATE:
        case NSEMessageType::BCAST_ONLY_MBP:
            return MessagePriority::CRITICAL;
            
        case NSEMessageType::BCAST_TICKER_AND_MKT_INDEX:
        case NSEMessageType::BCAST_ENHNCD_TICKER:
        case NSEMessageType::BCAST_LIMIT_PRICE_PROTECTION:
            return MessagePriority::HIGH;
            
        case NSEMessageType::BCAST_MW_ROUND_ROBIN:
        case NSEMessageType::BCAST_ENHNCD_MW_ROUND_ROBIN:
        case NSEMessageType::BCAST_INDICES:
        case NSEMessageType::BCAST_INDUSTRY_INDEX_UPDATE:
            return MessagePriority::NORMAL;
            
        default:
            return MessagePriority::LOW;
    }
}

inline MessagePriority getMessagePriority(BSEMessageType msgType) {
    switch (msgType) {
        case BSEMessageType::MARKET_PICTURE:
        case BSEMessageType::MARKET_PICTURE_COMPLEX:
        case BSEMessageType::PRODUCT_STATE_CHANGE:
            return MessagePriority::CRITICAL;
            
        case BSEMessageType::OPEN_INTEREST:
        case BSEMessageType::INDEX_CHANGE:
        case BSEMessageType::CLOSE_PRICE:
        case BSEMessageType::LIMIT_PRICE_PROTECTION:
            return MessagePriority::HIGH;
            
        default:
            return MessagePriority::NORMAL;
    }
}

} // namespace UDP

#endif // UDP_ENUMS_H
