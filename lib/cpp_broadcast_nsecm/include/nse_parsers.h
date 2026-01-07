#ifndef NSE_PARSERS_H
#define NSE_PARSERS_H

#include "nse_structures.h"
#include <cstdint>

namespace nsecm {

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

// TransCode 18708
void parse_message_18708(const MS_BCAST_BUY_BACK* msg);
void parse_bcast_buy_back(const MS_BCAST_BUY_BACK* msg);  // Alias

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

// TransCode 5295 (CTRL_MSG_TO_TRADER - Interactive)
void parse_message_5295(const MS_TRADER_INT_MSG* msg);
void parse_ctrl_msg_to_trader(const MS_TRADER_INT_MSG* msg);  // Alias

// TransCode 6013 (SECURITY_OPEN_PRICE - Broadcast)
void parse_message_6013(const MS_SEC_OPEN_MSGS* msg);
void parse_security_open_price(const MS_SEC_OPEN_MSGS* msg);  // Alias

// TransCode 6501
void parse_message_6501(const MS_BCAST_MESSAGE* msg);
void parse_jrnl_vct_msg(const MS_BCAST_MESSAGE* msg);  // Alias

// TransCode 7206
void parse_message_7206(const MS_BCAST_SYSTEM_INFORMATION* msg);
void parse_system_information(const MS_BCAST_SYSTEM_INFORMATION* msg);  // Alias

// TransCode 7210
void parse_message_7210(const MS_BCAST_CALL_AUCTION_ORD_CXL* msg);
void parse_call_auction_order_cxl(const MS_BCAST_CALL_AUCTION_ORD_CXL* msg);  // Alias

// TransCode 6511 (BC_OPEN_MSG)
void parse_message_6511(const BCAST_VCT_MESSAGES* msg);

// TransCode 6521 (BC_CLOSE_MSG)
void parse_message_6521(const BCAST_VCT_MESSAGES* msg);

// TransCode 6522 (BC_POSTCLOSE_MSG)
void parse_message_6522(const BCAST_VCT_MESSAGES* msg);

// TransCode 6531 (BC_PRE_OR_POST_DAY_MSG)
void parse_message_6531(const BCAST_VCT_MESSAGES* msg);

// TransCode 6541 (BC_CIRCUIT_CHECK)
void parse_message_6541(const MS_BC_CIRCUIT_CHECK* msg);

// TransCode 6571 (BC_NORMAL_MKT_PREOPEN_ENDED)
void parse_message_6571(const BCAST_VCT_MESSAGES* msg);

void parse_circuit_check(const MS_BC_CIRCUIT_CHECK* msg);
void parse_vct_messages(const BCAST_VCT_MESSAGES* msg);
void parse_symbol_status_change(const BC_SYMBOL_STATUS_CHANGE_ACTION* msg);

} // namespace nsecm

#endif // NSE_PARSERS_H
