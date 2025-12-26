# NSE Broadcast Message Test

## Overview

This test application listens to NSE UDP broadcast messages and displays detailed information for specific message codes:

- **7201** - Market Watch Round Robin (5 securities per packet)
- **7202** - Ticker and Market Index (17 securities per packet)
- **17201** - Enhanced Market Watch Round Robin (5 securities, 64-bit OI)
- **17202** - Enhanced Ticker and Market Index (12 securities, 64-bit OI)

## Message Details

### 7201 - Market Watch Round Robin
**Structure**: MS_BCAST_INQ_RESP_2 (472 bytes)
- Contains 5 securities in round-robin fashion
- 3 market types per security (Normal, Spot, Auction)
- Buy/Sell Volume and Price for each market type
- Open Interest (32-bit)

**Use Case**: Lightweight market watch display showing best bid/ask

### 7202 - Ticker and Market Index
**Structure**: MS_TICKER_TRADE_DATA (484 bytes)
- Contains up to 17 securities per packet
- Last trade: Price, Volume
- Open Interest tracking: Current, Day High, Day Low
- Market type indicator

**Use Case**: Real-time ticker tape, OI change monitoring

### 17201 - Enhanced Market Watch (64-bit OI)
**Structure**: MS_ENHNCD_BCAST_INQ_RESP_2 (492 bytes)
- Same as 7201 but with 64-bit Open Interest
- Required for high OI securities (>4.2 billion)

### 17202 - Enhanced Ticker (64-bit OI)
**Structure**: MS_ENHNCD_TICKER_TRADE_DATA (498 bytes)
- Same as 7202 but with 64-bit Open Interest
- Contains up to 12 securities per packet

## Building

```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp/build
cmake ..
cmake --build . --target test_broadcast_messages
```

## Running

### Basic Usage
```bash
cd build/tests/broadcast_message_test
./test_broadcast_messages
```

### With Custom Parameters
```bash
./test_broadcast_messages <multicast_ip> <port> <duration_seconds>

# Example: Listen to 233.129.186.1:55555 for 60 seconds
./test_broadcast_messages 233.129.186.1 55555 60
```

**Default values**:
- Multicast IP: 233.129.186.1
- Port: 55555
- Duration: 30 seconds

## Output Format

### Example Output for 7201 (Market Watch)

```
================================================================================
MESSAGE 7201 - MARKET WATCH ROUND ROBIN
================================================================================
Transaction Code: 7201
Sequence Number: 12345
Log Time: 34567890
Number of Records: 5
--------------------------------------------------------------------------------

Record #1 - Token: 45678
  Open Interest: 1234567
  Market Type 0:
    Buy:  Volume=     50000 Price=18500.50
    Sell: Volume=     45000 Price=18501.00
    Indicator: 0
  Market Type 1:
    Buy:  Volume=         0 Price=0.00
    Sell: Volume=         0 Price=0.00
    Indicator: 0
  Market Type 2:
    Buy:  Volume=         0 Price=0.00
    Sell: Volume=         0 Price=0.00
    Indicator: 0
```

### Example Output for 7202 (Ticker)

```
================================================================================
MESSAGE 7202 - TICKER AND MARKET INDEX
================================================================================
Transaction Code: 7202
Sequence Number: 12346
Log Time: 34567891
Number of Records: 17
--------------------------------------------------------------------------------

Record #1 - Token: 45678
  Market Type: 1
  Fill Price: 18500.75
  Fill Volume: 150
  Open Interest: 1234567
  Day High OI: 1250000
  Day Low OI: 1200000
```

## Progress Monitoring

The test prints periodic statistics every 5 seconds:

```
[5s] Current counts: 7201=45 7202=123 17201=0 17202=0
[10s] Current counts: 7201=92 7202=247 17201=0 17202=0
```

## Final Statistics

At the end of the test run:

```
================================================================================
MESSAGE STATISTICS
================================================================================
Total Messages Processed: 1234
--------------------------------------------------------------------------------
7201  (Market Watch):              456
7202  (Ticker & Index):            778
17201 (Enhanced Market Watch):     0
17201 (Enhanced Ticker & Index):   0
================================================================================
```

## Understanding the Data

### Price Conversion
All prices in the broadcast are in paise (×100). The test automatically converts:
- Raw value: 1850050 → Display: 18500.50

### Market Types (in 7201)
- **Type 0**: Normal Market
- **Type 1**: Spot/Auction Market
- **Type 2**: Odd Lot Market

### Open Interest
- **32-bit** (7201, 7202): Max ~4.2 billion
- **64-bit** (17201, 17202): Max ~9.2 quintillion

### Token Numbers
Each security has a unique token number. You can map these to symbols using the Masters files.

## Performance Notes

### Expected Message Rates
- **Normal Market Hours**: ~100-1000 messages/sec total
- **Volatile Period**: Up to 2000 messages/sec
- **TBT (Tick-by-Tick) Mode**: 2.3 million packets/sec (all message types)

### Message Distribution
- **7201**: Broadcasts continuously in round-robin (high frequency)
- **7202**: Only on actual trades (moderate frequency)
- **17201/17202**: Enhanced versions, may have lower frequency

## Troubleshooting

### No Messages Received
```bash
# Check if multicast is working
netstat -g | grep 233.129.186

# Check firewall
sudo iptables -L | grep 55555

# Verify UDP receiver is listening
sudo netstat -ulnp | grep 55555
```

### Only Seeing Certain Message Types
This is normal! Not all message types broadcast continuously:
- 7201 broadcasts constantly (round-robin through all securities)
- 7202 only when trades occur
- 17201/17202 may be used only for high OI securities

### Timestamps Look Wrong
Timestamps in the header are exchange timestamps. Convert log_time using appropriate timezone (IST).

## Integration with Trading Terminal

This test demonstrates the data structures used by the main trading terminal. The same parsing logic is used in:

```
src/services/FeedHandler.cpp
src/models/MarketWatchModel.cpp
```

## Next Steps

After understanding these message structures:
1. Integrate into MarketWatch for real-time updates
2. Add filtering by token numbers
3. Implement aggregation for statistics
4. Add latency tracking from broadcast timestamp

## References

- NSE F&O Broadcast Specification: `/lib/cpp_broacast_nsefo/bc_messagecode_list.md`
- Structure definitions: `/lib/cpp_broacast_nsefo/include/nse_market_data.h`
- Full protocol: `/docs/CM_protocol_full_text.txt`
