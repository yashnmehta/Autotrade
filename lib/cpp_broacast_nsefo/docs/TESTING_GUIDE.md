# NSE UDP Reader - Testing and Verification Guide

## Overview

This guide explains how to test the NSE UDP Reader and verify which message codes are being received from the NSE multicast feed.

---

## Quick Start

### 1. Run the Analyzer Script

The easiest way to test is using the provided analyzer script:

```bash
cd /home/ubuntu/Desktop/cpp_broacast_fo
./analyze_messages.sh
```

This will:
- Run the receiver for 60 seconds
- Capture all output
- Display statistics showing which message codes were received
- Save full output to `/tmp/nse_output.log`

### 2. Manual Testing

To run the receiver manually:

```bash
# Run with default config
./nse_reader config.ini

# Run for specific duration (e.g., 30 seconds)
timeout 30 ./nse_reader config.ini

# Run with debug logging
# Edit config.ini and set: log_level = DEBUG
./nse_reader config.ini
```

---

## Understanding the Output

### Statistics Display

When the receiver is running, you'll see periodic statistics like this:

```
====================================================================================================
NSE MULTICAST UDP RECEIVER - STATISTICS
====================================================================================================
Runtime: 30s
Total Packets: 1234
Compressed: 800 | Decompressed: 795 | Failures: 5
Total Bytes: 45.67 MB

Code   Name                            Count       Comp(KB)    Raw(KB)
--------------------------------------------------------------------------------
7200   BCAST_MBO_MBP_UPDATE           450         12.34       45.67
7201   BCAST_MW_ROUND_ROBIN           320         8.90        23.45
7202   BCAST_TICKER_AND_MKT_INDEX     234         6.78        18.90
7207   BCAST_INDICES                  120         3.45        10.23
7305   BCAST_SECURITY_MSTR_CHG        45          1.23        4.56
```

### Column Descriptions

- **Code**: Transaction code (e.g., 7200, 7201)
- **Name**: Human-readable message name
- **Count**: Number of messages received
- **Comp(KB)**: Compressed size in kilobytes
- **Raw(KB)**: Decompressed/raw size in kilobytes

---

## Message Codes Reference

### Market Data Messages (7200-7220)

| Code | Name | Description | Expected Frequency |
|------|------|-------------|-------------------|
| 7200 | BCAST_MBO_MBP_UPDATE | Market By Order/Price | Very High |
| 7201 | BCAST_MW_ROUND_ROBIN | Market Watch | High |
| 7202 | BCAST_TICKER_AND_MKT_INDEX | Ticker & Index | High |
| 7207 | BCAST_INDICES | Multiple Indices | Medium |
| 7208 | BCAST_ONLY_MBP | Market By Price Only | High |
| 7211 | BCAST_SPD_MBP_DELTA | Spread MBP Delta | Medium |
| 7220 | BCAST_LIMIT_PRICE_PROTECTION_RANGE | Price Limits | Low |

### Administrative Messages (6500-6600)

| Code | Name | Description | Expected Frequency |
|------|------|-------------|-------------------|
| 6501 | BCAST_JRNL_VCT_MSG | General Broadcast | Low |
| 6511 | BC_OPEN_MSG | Market Open | Once per day |
| 6521 | BC_CLOSE_MSG | Market Close | Once per day |
| 6541 | BC_CIRCUIT_CHECK | Circuit Breaker | Periodic |

### Database Updates (7300-7350)

| Code | Name | Description | Expected Frequency |
|------|------|-------------|-------------------|
| 7304 | UPDATE_LOCALDB_DATA | DB Data Update | Low |
| 7305 | BCAST_SECURITY_MSTR_CHG | Security Master | Low |
| 7306 | BCAST_PART_MSTR_CHG | Participant Master | Low |
| 7307 | UPDATE_LOCALDB_HEADER | DB Update Header | Low |
| 7308 | UPDATE_LOCALDB_TRAILER | DB Update Trailer | Low |
| 7324 | BCAST_INSTR_MSTR_CHG | Instrument Master | Low |
| 7325 | BCAST_INDEX_MSTR_CHG | Index Master | Low |

### Enhanced Messages (17000+)

| Code | Name | Description | Expected Frequency |
|------|------|-------------|-------------------|
| 17201 | BCAST_ENHNCD_MW_ROUND_ROBIN | Enhanced Market Watch | High |
| 17202 | BCAST_ENHNCD_TICKER_AND_MKT_INDEX | Enhanced Ticker | High |
| 17130 | ENHNCD_MKT_MVMT_CM_OI_IN | Enhanced Market Movement | Medium |

---

## Troubleshooting

### No Messages Received

**Symptom:** Statistics show 0 packets received

**Possible Causes:**

1. **Market is Closed**
   - NSE F&O trading hours: 9:15 AM - 3:30 PM IST (Monday-Friday)
   - Pre-open: 9:00 AM - 9:15 AM
   - Solution: Run during market hours

2. **Network Connectivity**
   - Check if multicast packets are reaching your system:
   ```bash
   sudo tcpdump -i any -n host 233.1.2.5 and udp port 64555
   ```
   - If no packets: Check network routing and switches

3. **Firewall Blocking**
   - Check firewall rules:
   ```bash
   sudo iptables -L -n | grep 64555
   sudo ufw status
   ```
   - Allow UDP port 64555:
   ```bash
   sudo ufw allow 64555/udp
   ```

4. **Multicast Routing**
   - Check multicast routes:
   ```bash
   ip maddr show
   netstat -g
   ```
   - Add multicast route if needed:
   ```bash
   sudo route add -net 233.0.0.0 netmask 255.0.0.0 dev eth0
   ```

### High Decompression Failures

**Symptom:** Statistics show many decompression failures

**Possible Causes:**
- Packet corruption
- Network issues
- Buffer size too small

**Solutions:**
1. Increase socket buffer size in config.ini
2. Check network quality
3. Enable debug logging to see specific errors

### Unknown Message Codes

**Symptom:** Statistics show `UNKNOWN_XXXX` codes

**Possible Causes:**
- New message types not yet implemented
- Protocol version mismatch
- Corrupted packets

**Solutions:**
1. Check if the code is in the NSE protocol document
2. Update constants.h if it's a valid new code
3. Report the code for investigation

---

## Network Verification

### Check Multicast Membership

```bash
# View multicast group memberships
ip maddr show

# Should show something like:
# 2:  eth0
#     link  33:33:00:00:00:01
#     link  01:00:5e:01:02:05  <-- This is 233.1.2.5
#     inet  233.1.2.5
```

### Capture Packets with tcpdump

```bash
# Capture multicast packets
sudo tcpdump -i any -n -v host 233.1.2.5 and udp port 64555

# Save to file for analysis
sudo tcpdump -i any -n -w /tmp/nse_capture.pcap host 233.1.2.5 and udp port 64555

# Analyze with Wireshark
wireshark /tmp/nse_capture.pcap
```

### Test Network Path

```bash
# Check if interface supports multicast
ip link show eth0 | grep MULTICAST

# Check routing
ip route show

# Test UDP connectivity (if you have a test sender)
nc -u 233.1.2.5 64555
```

---

## Performance Testing

### Measure Packet Rate

```bash
# Run for 60 seconds and calculate rate
./nse_reader config.ini 2>&1 | grep "Total Packets" | tail -1
```

### Monitor System Resources

```bash
# In another terminal while receiver is running
top -p $(pgrep nse_reader)

# Or use htop
htop -p $(pgrep nse_reader)

# Monitor network
iftop -i eth0 -f "host 233.1.2.5"
```

### Check for Packet Loss

```bash
# Compare received vs expected
# Look for gaps in sequence numbers (future enhancement)
```

---

## Expected Results

### During Market Hours

You should see:
- **High frequency** messages (7200, 7201, 7202, 7208): Hundreds to thousands per minute
- **Medium frequency** messages (7207, 7211): Tens to hundreds per minute
- **Low frequency** messages (7305, 7306, etc.): A few per minute
- **Administrative** messages (6511, 6521): At market open/close

### Pre-Market (9:00-9:15 AM)

You should see:
- Pre-open messages
- Security status updates
- Index updates
- Lower overall volume

### After Market Hours

You should see:
- Very few or no messages
- Possibly some administrative messages
- Database updates

---

## Logging and Debugging

### Enable Debug Logging

Edit `config.ini`:
```ini
[Logging]
log_level = DEBUG
log_file = /tmp/nse_reader.log
log_to_console = true
```

### View Logs

```bash
# Real-time log viewing
tail -f /tmp/nse_reader.log

# Search for specific message codes
grep "7200" /tmp/nse_reader.log

# Count occurrences
grep -c "BCAST_MBO_MBP" /tmp/nse_reader.log
```

### Enable Hex Dump

For deep debugging, enable hex dump in config.ini:
```ini
[Debug]
enable_hex_dump = true
hex_dump_size = 128
```

**Warning:** This generates a lot of output!

---

## Sample Test Session

Here's a complete test session:

```bash
# 1. Navigate to project directory
cd /home/ubuntu/Desktop/cpp_broacast_fo

# 2. Ensure binary is built
make

# 3. Check configuration
cat config.ini

# 4. Test network connectivity
sudo tcpdump -i any -n -c 10 host 233.1.2.5 and udp port 64555

# 5. Run analyzer script
./analyze_messages.sh

# 6. Review results
cat /tmp/nse_output.log

# 7. If no data, check market hours
date
# NSE hours: 9:15 AM - 3:30 PM IST (Mon-Fri)
```

---

## Interpreting Results

### Healthy Operation

```
Total Packets: 5000+
Compressed: ~80% of total
Decompressed: ~99% of compressed
Failures: < 1%
Multiple message codes (7200, 7201, 7202, etc.)
```

### Issues

```
Total Packets: 0
→ Check network connectivity

Failures: > 10%
→ Check network quality, increase buffer size

Only one message code
→ Possible filtering or partial feed

UNKNOWN codes
→ New protocol version or corruption
```

---

## Next Steps After Testing

Once you've verified message reception:

1. **Implement Parsers**
   - Create parser functions for each message type
   - Extract and process message data

2. **Add Data Storage**
   - Store parsed data to database
   - Implement time-series storage

3. **Build Analytics**
   - Calculate market statistics
   - Generate reports

4. **Set Up Monitoring**
   - Implement Prometheus metrics
   - Create Grafana dashboards
   - Configure alerts

---

## Support and Resources

### Documentation
- `docs/CODEBASE_ANALYSIS.md` - Code structure analysis
- `docs/PRODUCTION_DEPLOYMENT_GUIDE.md` - Deployment procedures
- `docs/MONITORING_ALERTING_PLAN.md` - Monitoring setup
- `nse_trimm_protocol_fo.md` - NSE protocol specification

### Useful Commands

```bash
# Quick test
./analyze_messages.sh

# Long-running test
timeout 300 ./nse_reader config.ini > /tmp/nse_5min.log 2>&1

# Background operation
nohup ./nse_reader config.ini > /tmp/nse.log 2>&1 &

# Stop background process
pkill nse_reader
```

---

## Conclusion

This testing guide provides comprehensive instructions for verifying NSE UDP Reader operation and analyzing received message codes. Regular testing during market hours will help ensure the system is working correctly and receiving all expected message types.

For production deployment, refer to `docs/PRODUCTION_DEPLOYMENT_GUIDE.md`.

---

**Last Updated:** 2025-12-17  
**Version:** 1.0
