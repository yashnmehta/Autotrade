# XTS Feed Bridge — Internet-Only (No UDP) Architecture

## Problem Statement

The trading terminal currently supports two market data sources:

1. **UDP Multicast** — Ultra-low-latency exchange broadcasts received via
   `UdpBroadcastService` → `FeedHandler` → UI. This is the primary data
   source in office/co-located environments.

2. **XTS WebSocket** — Socket.IO-based real-time feed from the XTS API.
   Currently used only for chart candle data (1505 events) and initial
   snapshots after login.

**For internet/home users**, UDP multicast is unreachable. These users need
ALL price data (MarketWatch, OptionChain, ATMWatch, SnapQuote, strategies)
to come through the XTS WebSocket. However, XTS has critical limits:

| Constraint               | Limit                          |
|--------------------------|--------------------------------|
| Tokens per segment       | ~200 (varies by subscription)  |
| REST API calls per sec   | ~10 (rate-limited, HTTP 429)   |
| Subscription batch size  | ~50 instruments per call       |
| WebSocket events         | Only for subscribed tokens     |

## Solution: XTSFeedBridge

A new singleton service (`XTSFeedBridge`) acts as an intelligent bridge
between the application's subscription system and the XTS REST API.

### Data Flow Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                        APPLICATION COMPONENTS                        │
│  MarketWatch   OptionChain   ATMWatch   SnapQuote   Strategies      │
└──────┬──────────────┬───────────┬──────────┬──────────┬─────────────┘
       │              │           │          │          │
       └──────────────┴───────────┴──────────┴──────────┘
                             │
                    FeedHandler::subscribe(segment, token)
                             │
              ┌──────────────┴──────────────┐
              │                             │
    UdpBroadcastService          XTSFeedBridge::requestSubscribe()
    ::subscribeToken()                      │
              │                    ┌────────┴────────┐
              │                    │   Feed Mode?    │
              │                    └──┬──────────┬───┘
              │                 HYBRID│          │XTS_ONLY
              │                (no-op)│          │
              │                       │   ┌──────┴──────────┐
              │                       │   │  Queue + Batch  │
              │                       │   │  Rate Limiter   │
              │                       │   │  LRU Eviction   │
              │                       │   └──────┬──────────┘
              │                       │          │
              │                       │   XTSMarketDataClient
              │                       │   ::subscribe(REST)
              │                       │          │
              │                       │   XTS WebSocket ──── ticks
              │                       │          │
              ▼                       │          ▼
        UDP::MarketTick               │   XTS::Tick
              │                       │          │
              │                       │   MainWindow::onTickReceived()
              │                       │          │
              │                       │   Convert to UDP::MarketTick
              │                       │          │
              └───────────────────────┴──────────┘
                             │
                    FeedHandler::onUdpTickReceived()
                             │
                    TokenPublisher::publish()
                             │
                    ┌────────┴────────────┐
                    │   All UI Widgets    │
                    │  (same signal path) │
                    └─────────────────────┘
```

### Key Design Decisions

#### 1. Zero UI Changes
The bridge operates entirely below the `FeedHandler` layer. UI components
(MarketWatch, OptionChain, etc.) continue to call `FeedHandler::subscribe()`
exactly as before. They never know whether data comes from UDP or XTS.

#### 2. Batched REST Calls with Rate Limiting
Instead of calling `XTSMarketDataClient::subscribe()` for every token
individually (which would hit the rate limit instantly), the bridge:
- Queues tokens in a FIFO
- Drains the queue every `batch_interval_ms` (default 200ms)
- Sends up to `batch_size` tokens (default 50) per REST call
- Tracks calls-per-second and pauses at `max_rest_calls_per_sec`
- On HTTP 429 (rate limit), enters a configurable cooldown period

#### 3. LRU Eviction for Token Cap
XTS limits each segment to ~200 subscribed tokens. When the cap is reached:
- The **least-recently-used** token is evicted (unsubscribed via REST)
- The new token takes its slot
- Evicted tokens stop receiving ticks until re-subscribed
- Every `FeedHandler::subscribe()` call "touches" the token, moving it to
  the back of the LRU list

This means actively viewed tokens (in visible MarketWatch rows, open
OptionChain) are never evicted, while tokens from closed windows or
scrolled-away rows are candidates for eviction.

#### 4. Mode Detection from Config
```ini
[FEED]
mode = hybrid      # Office/co-located (UDP + XTS WebSocket)
# mode = xts_only  # Internet/home (XTS WebSocket only)
```

When `mode = xts_only`:
- `setupNetwork()` skips `startBroadcastReceiver()` entirely
- `XTSFeedBridge` activates and starts processing the subscription queue
- All ticks flow through `XTSMarketDataClient::tickReceived` signal

When `mode = hybrid`:
- `XTSFeedBridge::requestSubscribe()` is a no-op (returns immediately)
- UDP receivers start as normal
- XTS WebSocket supplements with OHLC candles for charts

## Configuration Reference

```ini
[FEED]
# Feed mode: hybrid | xts_only
mode = hybrid

# XTS subscription limits (only active in xts_only mode)
max_tokens_per_segment    = 200    # Per-segment cap
max_rest_calls_per_sec    = 10     # REST rate limit
batch_size                = 50     # Tokens per REST call
batch_interval_ms         = 200    # Timer interval for queue drain
cooldown_on_rate_limit_ms = 5000   # Back-off on HTTP 429
```

## Files Changed

| File | Change |
|------|--------|
| `include/services/XTSFeedBridge.h` | **NEW** — Bridge singleton header |
| `src/services/XTSFeedBridge.cpp` | **NEW** — Bridge implementation |
| `include/utils/ConfigLoader.h` | Added feed config getters |
| `src/utils/ConfigLoader.cpp` | Implemented feed config getters |
| `src/services/FeedHandler.cpp` | `registerTokenWithUdpService()` now also calls `XTSFeedBridge::requestSubscribe()` |
| `src/app/MainWindow/Network.cpp` | Added `initializeXTSFeedBridge()`, skip UDP in XTS_ONLY mode |
| `src/app/MainWindow/MainWindow.cpp` | Calls `initializeXTSFeedBridge()` from `setConfigLoader()` |
| `include/app/MainWindow.h` | Added `initializeXTSFeedBridge()` declaration |
| `CMakeLists.txt` | Added XTSFeedBridge to build |
| `configs/config.ini` | Added `[FEED]` section |

## Usage: Switching to Internet-Only Mode

1. Edit `configs/config.ini`:
   ```ini
   [FEED]
   mode = xts_only
   ```

2. Restart the application.

3. The status bar will show XTS subscription counts:
   ```
   XTS Subs: 45 active, 12 pending (cap: 800)
   ```

4. If a rate limit is hit:
   ```
   ⚠ XTS rate limit hit — pausing 5s
   ```

## Performance Characteristics

| Metric                      | HYBRID Mode     | XTS_ONLY Mode        |
|-----------------------------|-----------------|----------------------|
| Tick latency                | ~50μs (UDP)     | ~5-50ms (WebSocket)  |
| Max concurrent tokens       | Unlimited       | 200/segment (800 total) |
| Subscription setup time     | Instant         | 200ms-5s (batched)   |
| Rate limit risk             | None            | Managed by bridge    |
| CPU overhead                | ~2% (UDP parse) | ~0.5% (JSON parse)   |
| Network bandwidth           | ~10 MB/s (all)  | ~0.5 MB/s (subscribed only) |

## Future Enhancements

1. **Auto-detect UDP availability**: Probe multicast group on startup;
   if no packets within 3s, auto-switch to XTS_ONLY mode.

2. **Priority subscriptions**: Mark certain tokens (e.g., underlying indices)
   as "never evict" to guarantee continuous data for strategy engines.

3. **Subscription sharing**: If multiple windows subscribe to the same token,
   count it only once against the XTS cap (already handled by `activeSet`).

4. **Graceful degradation UI**: Show a warning banner when >80% of capacity
   is used, suggesting the user close unnecessary windows.
