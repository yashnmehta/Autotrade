#ifndef NSE_PARSERS_H
#define NSE_PARSERS_H

#include "nse_structures.h"
#include <cstdint>

// ============================================================================
// MARKET DATA MESSAGE PARSERS
// ============================================================================

// TransCode 7200
void parse_message_7200(const MS_BCAST_MBO_MBP* msg);
void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg);

// TransCode 7208
void parse_message_7208(const MS_BCAST_ONLY_MBP* msg);
void parse_bcast_only_mbp(const MS_BCAST_ONLY_MBP* msg);

// TransCode 18703
void parse_message_18703(const MS_TICKER_TRADE_DATA* msg);
void parse_ticker_trade_data(const MS_TICKER_TRADE_DATA* msg);

// TransCode 7201
void parse_message_7201(const MS_BCAST_INQ_RESP_2* msg);
void parse_market_watch(const MS_BCAST_INQ_RESP_2* msg);

// ============================================================================
// INDEX MESSAGE PARSERS
// ============================================================================

// TransCode 7207
void parse_message_7207(const MS_BCAST_INDICES* msg);
void parse_bcast_indices(const MS_BCAST_INDICES* msg);

// TransCode 7203
void parse_message_7203(const MS_BCAST_INDUSTRY_INDICES* msg);
void parse_bcast_industry_indices(const MS_BCAST_INDUSTRY_INDICES* msg);

// ============================================================================
// ADMIN MESSAGE PARSERS
// ============================================================================

void parse_circuit_check(const MS_BC_CIRCUIT_CHECK* msg);
void parse_vct_messages(const BCAST_VCT_MESSAGES* msg);
void parse_jrnl_vct_msg(const MS_BCAST_MESSAGE* msg);
void parse_symbol_status_change(const BC_SYMBOL_STATUS_CHANGE_ACTION* msg);

#endif // NSE_PARSERS_H
