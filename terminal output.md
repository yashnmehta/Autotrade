UDP Packets
    │
    ▼
┌──────────────────────────┐
│  UdpBroadcastService     │  ← Parse all exchanges, emit XTS::Tick
│  tickReceived(Tick)      │
└──────────┬───────────────┘
           │
    ┌──────┴──────┐
    ▼             ▼
┌──────────┐  ┌──────────┐
│PriceCache│  │MainWindow│
│  .update │  │onTick    │
└──────────┘  └─────┬────┘
                    │
                    ▼
              ┌───────────┐
              │FeedHandler│ ← Route to token subscribers
              │ .onTick   │
              └─────┬─────┘
                    │
           ┌────────┼────────┐
           ▼        ▼        ▼
       [MW Rows] [SnapQt] [OptChn]


######################################


┌─────────────────────────────────────────────────────────────────────────────┐
│                           UDP BROADCAST DATA FLOW                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐        │
│  │   NSE FO     │ │   NSE CM     │ │   BSE FO     │ │   BSE CM     │        │
│  │  Multicast   │ │  Multicast   │ │  Multicast   │ │  Multicast   │        │
│  │   Receiver   │ │   Receiver   │ │   Receiver   │ │   Receiver   │        │
│  └───────┬──────┘ └───────┬──────┘ └───────┬──────┘ └───────┬──────┘        │
│          │                │                │                │               │
│          └────────────────┼────────────────┼────────────────┘               │
│                           │                │                                 │
│                           ▼                ▼                                 │
│                ┌─────────────────────────────────────┐                      │
│                │       UdpBroadcastService           │                      │
│                │  (Singleton - Converts to XTS::Tick)│                      │
│                │                                     │                      │
│                │   tickReceived(XTS::Tick) signal    │ ◄── UNIFIED SIGNAL   │
│                └──────────────────┬──────────────────┘                      │
│                                   │                                          │
│                    ┌──────────────┼─────────────────┐                       │
│                    │              │                 │                        │
│                    ▼              ▼                 ▼                        │
│             ┌──────────┐   ┌───────────┐    ┌──────────────┐                │
│             │MainWindow│   │PriceCache │    │Direct connect│                │
│             │(routing) │   │(storage)  │    │(SnapQuote)   │                │
│             └────┬─────┘   └───────────┘    └──────────────┘                │
│                  │                                                           │
│                  ▼                                                           │
│          ┌─────────────┐                                                    │
│          │ FeedHandler │ ◄── Token-based subscription                       │
│          │ (pub/sub)   │                                                    │
│          └──────┬──────┘                                                    │
│                 │                                                            │
│     ┌───────────┼───────────┐                                               │
│     ▼           ▼           ▼                                               │
│ ┌────────┐ ┌────────┐ ┌──────────┐                                         │
│ │ MW Row │ │ MW Row │ │OptionChn │                                         │
│ └────────┘ └────────┘ └──────────┘                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘



######################################


DP Broadcast Analysis & Snap Quote Roadmap
Executive Summary
Exchange	5-Level Depth	Parser Status	Service Status	Action Required
NSE FO	✅ In Protocol	✅ Fully Parsed	⚠️ Only level 0 used	Pass levels 1-4
NSE CM	✅ In Protocol	✅ Fully Parsed	⚠️ Only level 0 used	Pass levels 1-4
BSE FO/CM	✅ In Protocol	⚠️ Level 1 only	⚠️ Level 1 only	Parse levels 2-5
NSE FO Message Codes (from 
nse_market_data.h
 & protocol docs)
Code	Name	Structure	Data Provided	Parser	Integration
7200	BCAST_MBO_MBP_UPDATE	MS_BCAST_MBO_MBP (410 bytes)	LTP, OHLC, Volume, 5-level depth, TotalBuy/Sell	✅	✅
7201	BCAST_MW_ROUND_ROBIN	MS_BCAST_INQ_RESP_2 (472 bytes)	OI, 3 market levels (Normal/SL/Auction)	✅	⚠️ Not used
7202	BCAST_TICKER_AND_MKT_INDEX	MS_TICKER_TRADE_DATA (484 bytes)	Fill price/volume, OI, Day Hi/Lo OI	✅	✅
7207	BCAST_INDICES	MS_BCAST_INDICES (468 bytes)	Index values (6 indices)	❌	❌
7208	BCAST_ONLY_MBP	MS_BCAST_ONLY_MBP (470 bytes)	Same as 7200 (MBP only)	✅	✅
7211	BCAST_SPD_MBP_DELTA	MS_SPD_MKT_INFO (204 bytes)	Spread depth (5 levels bid/sell)	❌	❌
7220	BCAST_LIMIT_PRICE_PROTECTION	MS_BCAST_LIMIT_PRICE_PROTECTION_RANGE	High/Low exec bands	❌	❌
17201	BCAST_ENHNCD_MW_ROUND_ROBIN	MS_ENHNCD_BCAST_INQ_RESP_2 (492 bytes)	Enhanced MW (64-bit OI)	❌	❌
17202	BCAST_ENHNCD_TICKER	MS_ENHNCD_TICKER_TRADE_DATA (498 bytes)	Enhanced Ticker (64-bit OI)	❌	❌
5-Level Depth Structure (7200/7208)
From 
nse_market_data.h
:

struct ST_MBP_INFO {  // 10 bytes per level
    uint32_t qty;           // Quantity at this level
    uint32_t price;         // Price (divide by 100)
    uint16_t noOfOrders;    // Number of orders
};
// recordBuffer[0-4] = Bids, recordBuffer[5-9] = Asks
Currently Parsed: ✅ All 5 levels in 
parse_message_7200.cpp
 Currently Used: ⚠️ Only bids[0]/asks[0] in 
UdpBroadcastService.cpp

NSE CM Message Codes (from cpp_broadcast_nsecm)
Code	Name	Data Provided	Parser	Integration
7200	MBO/MBP Update	LTP, OHLC, Volume, 5-level depth (64-bit qty)	✅	✅
7201	Market Watch	OI, 3 market levels	✅	⚠️
7203	Industry Index	Industry index values	❌	❌
7207	Indices	Multiple indices	❌	❌
7208	Only MBP	MBP data (64-bit qty)	✅	✅
18703	Unknown	TBD	❌	❌
BSE Message Codes (from 
bse_manual.txt
 V5.0)
Code	Name	Data Provided	Parser	Integration
2001	Time Broadcast	Hour/Min/Sec/Msec	❌	❌
2002	Product State Change	Session number, Market type	❌	❌
2003	Auction Session Change	Auction session states	❌	❌
2004	News Headline	News URL	❌	❌
2011/2012	Index Change	Index values OHLC	✅	✅
2014	Close Price	Close price for instruments	❌	❌
2015	Open Interest	OI for derivatives	❌	❌
2016	VaR Percentage	Margin percentage	❌	❌
2017	Auction Market Picture	Auction order book (sell only)	❌	❌
2020	Market Picture	LTP, OHLC, Volume, 5-level depth	⚠️ L1 only	⚠️
2021	Market Picture (Complex)	Same as 2020 (64-bit token)	⚠️ L1 only	⚠️
2022	RBI Reference Rate	USD reference rate	❌	❌
2027	Odd-lot Market Picture	Odd-lot order book	❌	❌
2028	Implied Volatility	IV for options	❌	❌
2030	Auction Keep Alive	Network keep-alive	❌	❌
2033	Debt Market Picture	Debt instrument data	❌	❌
2034	Limit Price Protection	Circuit limits	❌	❌
2035	Call Auction Cancelled Qty	Cancelled quantity	❌	❌
BSE 5-Level Depth Structure (2020/2021)
From 
bse_manual.txt
:

Per depth level (repeats 5 times):
- Best Bid Rate:        Long (4 bytes)
- Total Bid Quantity:   Long Long (8 bytes)
- No. of Bid Orders:    Unsigned Long (4 bytes)
- Implied Buy Qty:      Long Long (8 bytes)
- Reserved:             Long (4 bytes)
- Best Offer Rate:      Long (4 bytes)
- Total Offer Qty:      Long Long (8 bytes)
- No. of Offer Orders:  Unsigned Long (4 bytes)
- Implied Sell Qty:     Long Long (8 bytes)
- Reserved:             Long (4 bytes)
Layout: Bid1, Ask1, Bid2, Ask2... (interleaved, 16 bytes each - simpler structure in raw UDP) Currently Parsed: ⚠️ Only level 1 in 
bse_receiver.cpp
 Required Change: Parse offsets 104-263 (5 levels × 2 sides × 16 bytes)

Implementation Roadmap
Phase 1: Extend XTS::Tick for 5-Level Depth
// In include/api/XTSTypes.h
struct Tick {
    // ... existing fields ...
    
    struct DepthLevel {
        double price = 0.0;
        int64_t quantity = 0;
        int orders = 0;
    };
    DepthLevel bidDepth[5];  // NEW
    DepthLevel askDepth[5];  // NEW
};
Phase 2: Update NSE FO/CM Callbacks
// In UdpBroadcastService.cpp - registerMarketDepthCallback
for (int i = 0; i < 5; i++) {
    tick.bidDepth[i].price = data.bids[i].price;
    tick.bidDepth[i].quantity = data.bids[i].quantity;
    tick.bidDepth[i].orders = data.bids[i].orders;
    tick.askDepth[i].price = data.asks[i].price;
    tick.askDepth[i].quantity = data.asks[i].quantity;
    tick.askDepth[i].orders = data.asks[i].orders;
}
Phase 3: Update BSE Parser for 5-Level Depth
// In bse_receiver.cpp - Parse all 5 levels
// Offsets: Bid1@104, Ask1@120, Bid2@136, Ask2@152, Bid3@168...
for (int level = 0; level < 5; level++) {
    int bidOffset = 104 + (level * 32);  // Each bid-ask pair is 32 bytes
    int askOffset = bidOffset + 16;       // Ask follows Bid (16 bytes each)
    
    DecodedDepthLevel bid, ask;
    bid.price = le32toh_func(*(uint32_t*)(record + bidOffset));
    bid.quantity = le32toh_func(*(uint32_t*)(record + bidOffset + 4));
    ask.price = le32toh_func(*(uint32_t*)(record + askOffset));
    ask.quantity = le32toh_func(*(uint32_t*)(record + askOffset + 4));
    
    decRec.bids.push_back(bid);
    decRec.asks.push_back(ask);
}
Phase 4: Connect Snap Quote to UDP Feed
// In SnapQuoteWindow.h
public slots:
    void onTickUpdate(const XTS::Tick& tick);
// In SnapQuoteWindow.cpp
void SnapQuoteWindow::onTickUpdate(const XTS::Tick& tick) {
    if ((int)tick.exchangeInstrumentID != m_token) return;
    
    updateQuote(tick.lastTradedPrice, tick.lastTradedQuantity, ...);
    
    for (int i = 0; i < 5; i++) {
        updateBidDepth(i+1, tick.bidDepth[i].quantity, 
                       tick.bidDepth[i].price, tick.bidDepth[i].orders);
        updateAskDepth(i+1, tick.askDepth[i].price,
                       tick.askDepth[i].quantity, tick.askDepth[i].orders);
    }
}
Phase 5: Implement 3-Window Limit
// In MainWindow::createSnapQuoteWindow()
if (countWindowsOfType("SnapQuote") >= 3) {
    // Future scope: Show notification
    return;
}
Files to Modify
File	Changes
XTSTypes.h
Add bidDepth[5]/askDepth[5] to Tick
UdpBroadcastService.cpp
Pass all 5 depth levels for NSE
bse_receiver.cpp
Parse all 5 depth levels
SnapQuoteWindow.h
Add 
onTickUpdate
 slot
[NEW] Data.cpp	Implement update methods and tick handler
Windows.cpp
Add 3-window limit
Additional Message Codes for Future Integration
NSE - Additional Data Sources
Code	Purpose	Priority
7201/17201	OI from Market Watch	Medium
7202/17202	OI updates (Hi/Lo OI)	Medium
7207	Index broadcast	Low
7130/17130	CM Asset OI	Low
BSE - Additional Data Sources
Code	Purpose	Priority
2015	Open Interest	Medium
2014	Close Price	Low
2034	Circuit Limits (DPR)	Low



##################################

Walkthrough: Bid/Ask Debug Logging
Changes Made
UdpBroadcastService.cpp
Added debug logging to NSEFO and NSECM market depth callbacks:

// NSEFO callback (line 44)
[UDP-NSEFO-DEPTH] Token:<token> Bid:<price>/<qty> Ask:<price>/<qty>
// NSECM callback (line 112)  
[UDP-NSECM-DEPTH] Token:<token> Bid:<price>/<qty> Ask:<price>/<qty>
Data.cpp
Extended debug logging from 20 to 100 ticks:

[MarketWatch] Tick <n> Token:<token> Segment:<seg> Bid:<price>(<qty>) Ask:<price>(<qty>)
How to Run
cd /home/ubuntu/Desktop/trading_terminal_cpp/build
./TradingTerminal 2>&1 | tee /tmp/bidask_debug.log
After running, add an NSEFO or NSECM scrip and watch the debug output. The logs will show:

[UDP-xxxx-DEPTH] - Raw data from UDP parser
[MarketWatch] Tick - Data received by MarketWatch
Compare values to see if corruption occurs between these checkpoints.

What to Look For
Issue	Symptom in Logs
Swapped bid/ask	Ask: shows lower price than Bid: or values match wrong
Zero values	Ask:0/0 when [UDP-...] shows non-zero values
No updates	[MarketWatch] logs never appear for the token



##################################################


UDP Packet (2015)
     │
     ▼
BSEReceiver::validatePacket()  ← Accepts 2015 message type
     │
     ▼
BSEReceiver::decodeAndDispatch()
     │
     ▼
BSEReceiver::decodeOpenInterest()  ← Parses token, OI qty, OI value, OI change
     │
     ▼
oiCallback_(DecodedOpenInterest)
     │
     ▼
UdpBroadcastService  ← Converts to XTS::Tick with openInterest field
     │
     ▼
tickReceived(tick)  ← Emits to subscribers
     │
     ▼
SnapQuoteWindow, MarketWatch, etc.


Summary: BSE Open Interest (2015) Implementation Complete ✅
Files Modified:
File	Changes
lib/cpp_broadcast_bsefo/include/bse_protocol.h
Added MSG_TYPE_OPEN_INTEREST = 2015, 
DecodedOpenInterest
 struct
lib/cpp_broadcast_bsefo/include/bse_receiver.h
Added packets2015 counter, 
OpenInterestCallback
, oiCallback_ member, 
decodeOpenInterest()
 declaration
lib/cpp_broadcast_bsefo/include/bse_utils.h
Added 
le64toh_func
 for 64-bit little-endian conversion
lib/cpp_broadcast_bsefo/src/bse_receiver.cpp
Added 2015 to validation, implemented 
decodeOpenInterest()
 function
src/services/UdpBroadcastService.cpp
Added OI callback for BSE FO to emit ticks with OI data
docs/PARSER_COVERAGE.md
Updated to mark 2015 as implemented



#################################


Enhance 
PriceCache
 to use composite keys (segment+token)?
yes


Create an architecture document with these patterns?
yes . create indepth document

Review and fix the build to complete the current implementation?
yes.


review exchange documents also as we have implemented parsers for only few message codes.
we have to have a complete solution 



############################


continue.


we have to consider overall architecture , data flow from  multiple udp readers to main application . how we forward it to subscriber objects and also keep a centralised store of live udp data (price cache).
do we create a single unifor live price signal for all the objects and use cases or we will create different signals with use case specific data to optimise the working 


This is an excellent architecture question. Let me analyze the current data flow and provide recommendations:

Current Architecture Analysis
Let me examine the existing data flow:

Analyzed

FeedHandler.h
Analyzed

PriceCache.h
Analyzed

UdpBroadcastService.h
Now I understand the current architecture. Let me provide a comprehensive analysis:

Current Architecture Overview
┌─────────────────────────────────────────────────────────────────────────────┐
│                           UDP BROADCAST DATA FLOW                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐        │
│  │   NSE FO     │ │   NSE CM     │ │   BSE FO     │ │   BSE CM     │        │
│  │  Multicast   │ │  Multicast   │ │  Multicast   │ │  Multicast   │        │
│  │   Receiver   │ │   Receiver   │ │   Receiver   │ │   Receiver   │        │
│  └───────┬──────┘ └───────┬──────┘ └───────┬──────┘ └───────┬──────┘        │
│          │                │                │                │               │
│          └────────────────┼────────────────┼────────────────┘               │
│                           │                │                                 │
│                           ▼                ▼                                 │
│                ┌─────────────────────────────────────┐                      │
│                │       UdpBroadcastService           │                      │
│                │  (Singleton - Converts to XTS::Tick)│                      │
│                │                                     │                      │
│                │   tickReceived(XTS::Tick) signal    │ ◄── UNIFIED SIGNAL   │
│                └──────────────────┬──────────────────┘                      │
│                                   │                                          │
│                    ┌──────────────┼─────────────────┐                       │
│                    │              │                 │                        │
│                    ▼              ▼                 ▼                        │
│             ┌──────────┐   ┌───────────┐    ┌──────────────┐                │
│             │MainWindow│   │PriceCache │    │Direct connect│                │
│             │(routing) │   │(storage)  │    │(SnapQuote)   │                │
│             └────┬─────┘   └───────────┘    └──────────────┘                │
│                  │                                                           │
│                  ▼                                                           │
│          ┌─────────────┐                                                    │
│          │ FeedHandler │ ◄── Token-based subscription                       │
│          │ (pub/sub)   │                                                    │
│          └──────┬──────┘                                                    │
│                 │                                                            │
│     ┌───────────┼───────────┐                                               │
│     ▼           ▼           ▼                                               │
│ ┌────────┐ ┌────────┐ ┌──────────┐                                         │
│ │ MW Row │ │ MW Row │ │OptionChn │                                         │
│ └────────┘ └────────┘ └──────────┘                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
My Recommendation: Unified Signal with Centralized Store
Based on your architecture, I recommend:

1. Keep Single Unified XTS::Tick Signal ✅
Reasons:

Simplicity: One data structure, one signal path
Consistency: All subscribers get the same data format
Already Implemented: UdpBroadcastService::tickReceived(XTS::Tick)
5-level depth is now included in XTS::Tick (from our changes)
2. Layered Architecture
Layer	Component	Responsibility
L1: Collection	
UdpBroadcastService
Receive UDP, parse, normalize to XTS::Tick
L2: Distribution	
FeedHandler
Token-based pub/sub for interested subscribers
L3: Storage	
PriceCache
Centralized store for LTP/depth (avoid 0.00 flash)
L4: Consumers	Windows	UI updates, calculations
3. Connection Patterns
cpp
// Pattern A: High-frequency consumers (MarketWatch rows)
// Use FeedHandler for token-filtered subscription
FeedHandler::instance().subscribe(segment, token, this, &Row::onTick);
// Pattern B: Single-token windows (SnapQuote, BuySell)  
// Direct connect to UdpBroadcastService (filter internally)
connect(&UdpBroadcastService::instance(), &tickReceived, this, &onTick);
// Pattern C: Data-hungry consumers (OrderBook, Chart)
// Subscribe via FeedHandler + initial seed from PriceCache
auto cached = PriceCache::instance().getPrice(token);
if (cached) updateUI(*cached);  // No 0.00 flash
FeedHandler::instance().subscribe(segment, token, this, &onTick);
4. PriceCache Enhancement Needed
Currently 
PriceCache
 is keyed by token only. It should use composite key like 
FeedHandler
:

cpp
// Change from:
std::unordered_map<int, CachedPrice> m_cache;
// To:
std::unordered_map<int64_t, CachedPrice> m_cache;  // compositeKey = segment << 32 | token
5. Why NOT Create Use-Case-Specific Signals?
Approach	Pros	Cons
Single Signal	Simple, consistent	Every subscriber filters
Multiple Signals	Less filtering	Complexity explosion, harder to maintain
Verdict: For 4 exchanges × 10k tokens each, the filtering cost (~10ns) is negligible compared to the maintenance burden of multiple signal paths.