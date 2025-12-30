#ifndef NSE_PARSERS_H
#define NSE_PARSERS_H

#include "nse_structures.h"
#include <cstdint>

namespace nsefo {

// ============================================================================
// MARKET DATA MESSAGE PARSERS
// ============================================================================

// TransCode 7200
void parse_message_7200(const MS_BCAST_MBO_MBP* msg);
void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg);

// TransCode 7208
void parse_message_7208(const MS_BCAST_ONLY_MBP* msg);
void parse_bcast_only_mbp(const MS_BCAST_ONLY_MBP* msg);

// TransCode 7202
void parse_message_7202(const MS_TICKER_TRADE_DATA* msg);
void parse_ticker_trade_data(const MS_TICKER_TRADE_DATA* msg);

// TransCode 17202
void parse_message_17202(const MS_ENHNCD_TICKER_TRADE_DATA* msg);
void parse_enhncd_ticker_trade_data(const MS_ENHNCD_TICKER_TRADE_DATA* msg);

// TransCode 7201
void parse_message_7201(const MS_BCAST_INQ_RESP_2* msg);
void parse_market_watch(const MS_BCAST_INQ_RESP_2* msg);

// TransCode 17201
void parse_message_17201(const MS_ENHNCD_BCAST_INQ_RESP_2* msg);
void parse_enhncd_market_watch(const MS_ENHNCD_BCAST_INQ_RESP_2* msg);

// TransCode 7211
void parse_message_7211(const MS_SPD_MKT_INFO* msg);
void parse_spd_mbp_delta(const MS_SPD_MKT_INFO* msg);

// TransCode 7220
void parse_message_7220(const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE* msg);
void parse_limit_price_protection(const MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE* msg);

// ============================================================================
// ADMIN MESSAGE PARSERS
// ============================================================================

void parse_circuit_check(const MS_BC_CIRCUIT_CHECK* msg);
void parse_market_open(const MS_BC_OPEN_MSG* msg);
void parse_market_close(const MS_BC_CLOSE_MSG* msg);

} // namespace nsefo

#endif // NSE_PARSERS_H
