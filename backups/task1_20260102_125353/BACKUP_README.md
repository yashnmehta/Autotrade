# Backup - Task 1: Before UDP Type Separation

**Created:** January 2, 2026 12:53:53  
**Purpose:** Backup before implementing UDP::MarketTick separation from XTS::Tick

## Files Backed Up

1. **XTSTypes.h** (5.9K)
   - Current unified structure mixing WebSocket + UDP data
   - Contains `XTS::Tick` with bidDepth[5], askDepth[5], latency tracking
   - Issue: Semantic confusion (close vs prevClose, ATP naming, etc.)

2. **bse_receiver.cpp** (16K)
   - BSE UDP parser - currently only parsing Level 1 depth
   - **Critical Gap:** Levels 2-5 exist in packets (offsets 136-263) but ignored
   - Decodes message 2020/2021 (MARKET_PICTURE), 2015 (OPEN_INTEREST), 2012 (INDEX)

3. **UdpBroadcastService.h** (4.0K)
   - Manages 4 receivers: NSE FO, NSE CM, BSE FO, BSE CM
   - Emits signals with `XTS::Tick` - will change to `UDP::MarketTick`

4. **FeedHandler.h** (5.1K)
   - Token-based pub/sub routing
   - Currently accepts `XTS::Tick` - will change to `UDP::MarketTick`

## Changes Planned

### Immediate (Task 2-3):
- Create `include/udp/UDPTypes.h` with `UDP::MarketTick` structure
- Create `include/udp/UDPEnums.h` with exchange segments, message types
- Separate UDP-specific fields from WebSocket fields

### Next (Task 4):
- Fix BSE 5-level depth parsing (add levels 2-5)
- Verify interleaved layout: Bid1, Ask1, Bid2, Ask2, ..., Bid5, Ask5

### Integration (Task 6-8):
- Update UdpBroadcastService to emit `UDP::MarketTick`
- Update FeedHandler to route `UDP::MarketTick`
- Update MarketWatchWindow to consume `UDP::MarketTick`

## Restore Instructions

If rollback needed:
```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp
cp backups/task1_20260102_125353/XTSTypes.h include/api/
cp backups/task1_20260102_125353/bse_receiver.cpp lib/cpp_broadcast_bsefo/src/
cp backups/task1_20260102_125353/UdpBroadcastService.h include/services/
cp backups/task1_20260102_125353/FeedHandler.h include/services/
```

## Reference

See: `UDP_BROADCAST_COMPREHENSIVE_ANALYSIS.md` for full roadmap.
