Perfect! Now let me create a comprehensive summary of all NSE broadcast message codes. Let me compile the complete list:

NSE F&O Broadcast Message Codes Summary
Based on the NSE documentation, here are all the broadcast message codes with their purpose and use:

Real-Time Market Data Messages
7200 - BCAST_MBO_MBP_UPDATE (Market By Order/Market By Price Update)

Provides 5 best buy/sell orders with full order book depth (MBO) + aggregated price levels (MBP)
Contains: LTP, volume, bid/ask prices, OHLC, total buy/sell quantities, open interest
Use: Primary real-time price feed for order book display and market depth analysis




7208 - BCAST_ONLY_MBP (Market By Price Only)

Similar to 7200 but sends only aggregated price levels (no individual trader IDs)
Lighter weight, supports 2 securities per packet, includes pre-open session data
Use: Efficient market depth for applications not needing full MBO data
7201 - BCAST_MW_ROUND_ROBIN (Market Watch Round Robin)

Compact market watch data: best bid/ask, LTP, volume, open interest for 5 securities per packet
Broadcasts continuously in round-robin fashion across all securities
Use: Lightweight ticker for displaying multiple scrips in market watch windows
7202 - BCAST_TICKER_AND_MKT_INDEX (Ticker and Market Index)

Trade ticker data: last trade price, volume, open interest changes for up to 17 securities
Includes day high/low OI tracking
Use: Real-time ticker tape display and OI tracking
7211 - BCAST_SPD_MBP_DELTA (Spread Market By Price)

Market depth for spread combinations (calendar spreads, inter-commodity)
Shows 5 best buy/sell levels for spread orders with price differentials
Use: Display spread order books and track spread trading activity
7220 - BCAST_LIMIT_PRICE_PROTECTION_RANGE

Dynamic price bands/circuit limits for up to 25 securities per packet
Contains upper and lower execution bands
Use: Validate order prices against exchange-defined price protection ranges



Index & Market Statistics
7207 - BCAST_INDICES (Multiple Index Broadcast)

Up to 6 indices with complete data: value, high/low, opening/closing, yearly high/low, market cap
Includes up/down move counts and percent change
Use: Display index values (NIFTY, BANKNIFTY, etc.) and track index movements
7203 - BCAST_INDUSTRY_INDEX_UPDATE

Industry-wise index values for up to 20 industry sectors
Contains industry name and current index value
Use: Sector-wise market analysis and heat maps
7732 - GI_INDICES_ASSETS (Global Indices)

Global market indices data (e.g., Dow Jones, Nasdaq, Hang Seng)
Contains OHLC, close, previous close, lifetime high/low
Use: Display international market indicators
7733 - GI_CONTRACT_ASSETS (Global Contracts)

Global index derivative contracts data
Full contract details: strikes, bid/ask, OHLC, OI, total trades
Use: Track global derivative instruments
7130 - MKT_MVMT_CM_OI_IN (Underlying Open Interest)

Aggregated open interest for up to 58 underlying assets
Contains token and current OI for each underlying
Use: Track total OI across all expiries for an underlying (e.g., total NIFTY OI)
Master Data & Configuration Updates
7305 - BCAST_SECURITY_MSTR_CHG

Real-time updates when security master data changes
Contains complete contract specifications, price bands, lot sizes
Use: Keep local security master database synchronized
7320 - BCAST_SECURITY_STATUS_CHG

Trading status changes (open, suspended, pre-open, auction)
Security-level status updates
Use: Display trading status indicators and handle trading halts
7210 - BCAST_STOCK_STATUS_CHG_PREOPEN

Trading status changes specific to pre-open session
Similar to 7320 but for pre-open phase
Use: Handle pre-open session status transitions
7309 - BCAST_SPD_MSTR_CHG

Spread combination master updates
Contains spread definitions and allowed spread types
Use: Maintain spread combination catalog for spread order entry
7306 - BCAST_PART_MSTR_CHG

Participant (broker/member) master changes
Updates to trading member information
Use: Keep broker/member database current
7324 - BCAST_INSTR_MSTR_CHG

Instrument master changes
Updates to instrument definitions and properties
Use: Maintain instrument type catalog
7325 - BCAST_INDEX_MSTR_CHG

Index master updates
Changes to index definitions and constituents
Use: Keep index configuration synchronized
7326 - BCAST_INDEX_MAP_TABLE

Index constituent mapping (which stocks are in which indices)
Links securities to their parent indices
Use: Build index composition data for analysis
System & Control Messages
6541 - BCAST_CIRCUIT_MSG

Heartbeat/pulse message sent every 9 seconds when no data to broadcast
Indicates broadcast circuit is alive
Use: Monitor broadcast connection health and detect disconnections
6013 - SECURITY_OPEN_PRICE

Opening price announcement when market opens
Contains symbol, series, token, and opening price
Use: Display market open price notifications
Enhanced Versions (17xxx series)
Several messages have enhanced versions with 64-bit support for large values:

17202 - BCAST_ENHNCD_TICKER_AND_MKT_INDEX (enhanced OI fields)
17201 - BCAST_ENHNCD_MW_ROUND_ROBIN (enhanced OI fields)
17130 - ENHNCD_MKT_MVMT_CM_OI_IN (64-bit OI support)
Priority Integration Order:

7200/7208 - Core price feed (essential)
7207 - Index data (important)
7201 - Market watch feed (important)
7202 - Ticker feed (useful)
7305/7320 - Master updates (important)
7220 - Price protection (risk management)
7130 - Underlying OI (analytics)
7211 - Spread data (if spread trading needed)
The cpp_broadcast library already has structures defined for most of these. Ready to integrate?