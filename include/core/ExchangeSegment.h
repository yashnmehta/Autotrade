#ifndef CORE_EXCHANGE_SEGMENT_H
#define CORE_EXCHANGE_SEGMENT_H

#include <QMetaType>
#include <QString>
#include <cstdint>

/**
 * @file ExchangeSegment.h
 * @brief Canonical exchange segment enum — the SINGLE SOURCE OF TRUTH
 *
 * This replaces all previous duplicate enums:
 *   - XTS::ExchangeSegment  (include/api/xts/XTSTypes.h)       — REMOVED
 *   - UDP::ExchangeSegment  (include/udp/UDPEnums.h)           — REMOVED
 *   - SymbolSegment          (include/strategy/model/StrategyTemplate.h) — REMOVED
 *   - ExchangeReceiver       (include/services/UdpBroadcastService.h)    — REMOVED
 *
 * Numeric values match the XTS API codes, which are also used by
 * RepositoryManager, FeedHandler, XTSFeedBridge, and all struct fields
 * (InstrumentData::exchangeSegment, SymbolBinding::segment, etc.).
 *
 * Usage:
 *   #include "core/ExchangeSegment.h"
 *   ExchangeSegment seg = ExchangeSegment::NSEFO;
 *   QString name = ExchangeSegmentUtil::toString(seg);
 *   bool isFO = ExchangeSegmentUtil::isDerivative(seg);
 */

// ═══════════════════════════════════════════════════════════════════
//  Canonical Enum
// ═══════════════════════════════════════════════════════════════════

enum class ExchangeSegment : int {
    Unknown = 0,   // Invalid / unset
    NSECM   = 1,   // NSE Cash Market (Equities)
    NSEFO   = 2,   // NSE Futures & Options (Derivatives)
    NSECD   = 3,   // NSE Currency Derivatives
    BSECM   = 11,  // BSE Cash Market (Equities)
    BSEFO   = 12,  // BSE Futures & Options (Derivatives)
    MCXFO   = 51,  // MCX Commodity Derivatives
    BSECD   = 61   // BSE Currency Derivatives
};

// ═══════════════════════════════════════════════════════════════════
//  Utility Functions
// ═══════════════════════════════════════════════════════════════════

namespace ExchangeSegmentUtil {

// ─── String Conversion ───────────────────────────────────────────

/**
 * @brief Convert segment to human-readable key (e.g. "NSEFO", "BSECM")
 */
inline QString toString(ExchangeSegment seg) {
    switch (seg) {
        case ExchangeSegment::NSECM: return QStringLiteral("NSECM");
        case ExchangeSegment::NSEFO: return QStringLiteral("NSEFO");
        case ExchangeSegment::NSECD: return QStringLiteral("NSECD");
        case ExchangeSegment::BSECM: return QStringLiteral("BSECM");
        case ExchangeSegment::BSEFO: return QStringLiteral("BSEFO");
        case ExchangeSegment::MCXFO: return QStringLiteral("MCXFO");
        case ExchangeSegment::BSECD: return QStringLiteral("BSECD");
        default: return QStringLiteral("UNKNOWN(%1)").arg(static_cast<int>(seg));
    }
}

/**
 * @brief Parse a segment key string (e.g. "NSEFO", "BSECM") to enum
 */
inline ExchangeSegment fromString(const QString& s) {
    if (s == QLatin1String("NSECM"))  return ExchangeSegment::NSECM;
    if (s == QLatin1String("NSEFO"))  return ExchangeSegment::NSEFO;
    if (s == QLatin1String("NSECD"))  return ExchangeSegment::NSECD;
    if (s == QLatin1String("BSECM"))  return ExchangeSegment::BSECM;
    if (s == QLatin1String("BSEFO"))  return ExchangeSegment::BSEFO;
    if (s == QLatin1String("MCXFO"))  return ExchangeSegment::MCXFO;
    if (s == QLatin1String("BSECD"))  return ExchangeSegment::BSECD;
    return ExchangeSegment::Unknown;
}

// ─── Int Conversion ──────────────────────────────────────────────

/**
 * @brief Convert raw int (XTS API / UDP code) to ExchangeSegment
 *
 * Values 1,2,3,11,12,51,61 are the XTS API standard and are used
 * consistently across UDP, REST, WebSocket, and RepositoryManager.
 */
inline ExchangeSegment fromInt(int code) {
    switch (code) {
        case 1:  return ExchangeSegment::NSECM;
        case 2:  return ExchangeSegment::NSEFO;
        case 3:  return ExchangeSegment::NSECD;
        case 11: return ExchangeSegment::BSECM;
        case 12: return ExchangeSegment::BSEFO;
        case 51: return ExchangeSegment::MCXFO;
        case 61: return ExchangeSegment::BSECD;
        default: return ExchangeSegment::Unknown;
    }
}

/**
 * @brief Convert ExchangeSegment to its raw int value
 */
inline int toInt(ExchangeSegment seg) {
    return static_cast<int>(seg);
}

// ─── Category Helpers ────────────────────────────────────────────

/**
 * @brief Is this a derivatives (F&O) segment?
 */
inline bool isDerivative(ExchangeSegment seg) {
    return seg == ExchangeSegment::NSEFO ||
           seg == ExchangeSegment::BSEFO ||
           seg == ExchangeSegment::MCXFO ||
           seg == ExchangeSegment::NSECD ||
           seg == ExchangeSegment::BSECD;
}

/**
 * @brief Is this a cash/equity segment?
 */
inline bool isEquity(ExchangeSegment seg) {
    return seg == ExchangeSegment::NSECM ||
           seg == ExchangeSegment::BSECM;
}

/**
 * @brief Is this an NSE segment (any)?
 */
inline bool isNSE(ExchangeSegment seg) {
    return seg == ExchangeSegment::NSECM ||
           seg == ExchangeSegment::NSEFO ||
           seg == ExchangeSegment::NSECD;
}

/**
 * @brief Is this a BSE segment (any)?
 */
inline bool isBSE(ExchangeSegment seg) {
    return seg == ExchangeSegment::BSECM ||
           seg == ExchangeSegment::BSEFO ||
           seg == ExchangeSegment::BSECD;
}

/**
 * @brief Is this a valid (known) segment?
 */
inline bool isValid(ExchangeSegment seg) {
    return seg != ExchangeSegment::Unknown;
}

/**
 * @brief Get the exchange name (NSE, BSE, MCX)
 */
inline QString exchangeName(ExchangeSegment seg) {
    if (isNSE(seg))  return QStringLiteral("NSE");
    if (isBSE(seg))  return QStringLiteral("BSE");
    if (seg == ExchangeSegment::MCXFO) return QStringLiteral("MCX");
    return QStringLiteral("UNKNOWN");
}

/**
 * @brief Get the segment suffix (CM, FO, CD)
 */
inline QString segmentSuffix(ExchangeSegment seg) {
    switch (seg) {
        case ExchangeSegment::NSECM:
        case ExchangeSegment::BSECM: return QStringLiteral("CM");
        case ExchangeSegment::NSEFO:
        case ExchangeSegment::BSEFO:
        case ExchangeSegment::MCXFO: return QStringLiteral("FO");
        case ExchangeSegment::NSECD:
        case ExchangeSegment::BSECD: return QStringLiteral("CD");
        default: return QString();
    }
}

} // namespace ExchangeSegmentUtil

Q_DECLARE_METATYPE(ExchangeSegment)

#endif // CORE_EXCHANGE_SEGMENT_H
