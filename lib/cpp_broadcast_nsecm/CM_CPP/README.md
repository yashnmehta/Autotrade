# NSE Capital Market Multicast Receiver - C++ Implementation

This is a high-performance C++ implementation of the NSE Capital Market multicast UDP receiver, converted from the original Go implementation with modular design for easy extensibility.

## 🚀 Features

- **Ultra-fast LZO decompression** using optimized C++ with direct pointer manipulation
- **Modular message processing** - Easy to add new message codes
- **Real-time CSV export** with timestamps
- **Comprehensive statistics** including packet rates and message counts
- **Signal handling** for graceful shutdown (Ctrl+C)
- **Cross-platform compatibility** (Linux/Unix systems)

## 📁 Project Structure

```
CM_CPP/
├── main.cpp                      # Main entry point
├── lzo_decompressor_safe.cpp     # Ultra-optimized LZO decompressor
├── lzo_decompressor_safe.h       # LZO decompressor header
├── Makefile                      # Build configuration
├── README.md                     # This file
└── msg_codes/                    # Message-specific decoders
    ├── message_6501_live.cpp     # Message 6501 decoder
    └── message_6501_live.h       # Message 6501 header
```

## 🛠️ Building

### Prerequisites
- C++11 compatible compiler (g++ recommended)
- pthread library
- Unix/Linux environment with socket support

### Build Commands

```bash
# Build the main executable
make

# Clean build files
make clean

# Build with debug information
make debug

# Build optimized release version
make release

# Show all available targets
make help
```

## 🎯 Usage

### Command Line Interface

```bash
./nse_cm_receiver <multicast_ip> <port> <message_code>
```

### Parameters

- **multicast_ip**: NSE multicast IP address to listen on
- **port**: Port number to listen on  
- **message_code**: NSE message code to process

### Examples

#### Live Market Hours
```bash
# Process message 6501 during live market hours
./nse_cm_receiver 233.1.2.5 8222 6501

# Process message 6511 (Market Open) during live market hours
./nse_cm_receiver 233.1.2.5 8222 6511
```

#### After Market Hours
```bash
# Process message 6501 during after-market hours
./nse_cm_receiver 231.31.31.4 18901 6501
```

### Quick Start with Makefile
```bash
# Run live market example (message 6501)
make run-example

# Run market open example (message 6511)
make run-6511

# Run after-hours example
make run-afterhours
```

## 📊 Supported Message Codes

| Code | Description | Structure | Purpose |
|------|-------------|-----------|---------|
| 6501 | BCAST_JRNL_VCT_MSG (Journal/VCT Messages) | MS_TRADER_INT_MSG (298 bytes) | System messages, auction notifications, margin violations, listings |
| 6511 | BC_OPEN_MESSAGE (Market Open Notifications) | BCAST_VCT_MESSAGES (298 bytes) | Market open notification (9:15 AM start) |

*More message codes can be easily added by creating new files in the `msg_codes/` directory.*

## 📈 Output

The receiver creates CSV files in the `csv_output/` directory with the format:
```
message_[CODE]_[TIMESTAMP].csv
```

### Message 6501 CSV Format
```csv
Timestamp,TransactionCode,BranchNumber,BrokerNumber,ActionCode,MsgLength,Message
```

### Message 6511 CSV Format
```csv
Timestamp,TransactionCode,BranchNumber,BrokerNumber,ActionCode,TraderWsBit,MsgLength,Message
```

## 🔧 Adding New Message Codes

1. **Create decoder files** in `msg_codes/`:
   - `message_XXXX_live.cpp` - Implementation
   - `message_XXXX_live.h` - Header

2. **Implement required functions**:
   ```cpp
   bool runMessageXXXXReceiver(const std::string& multicastIP, int port);
   void stopMessageXXXXReceiver();
   ```

3. **Update main.cpp** to include the new message code:
   ```cpp
   #include "msg_codes/message_XXXX_live.h"
   
   // Add case in runReceiver() function
   case XXXX:
       return runMessageXXXXReceiver(multicastIP, port);
   ```

4. **Update Makefile** to include new source files

## ⚡ Performance

The C++ implementation is optimized for maximum performance:

- **LZO Decompression**: 270-300 MB/s target throughput
- **Zero-copy operations** where possible
- **Direct pointer arithmetic** for packet processing
- **Minimal memory allocations** in hot paths
- **Efficient CSV writing** with buffered output

## 🛡️ Safety and Reliability

- **Exception handling** for LZO decompression errors
- **Graceful shutdown** with signal handlers (Ctrl+C)
- **Input validation** for all packet data
- **Bounds checking** for critical operations
- **Atomic counters** for thread-safe statistics

## 📊 Real-time Statistics

During operation, the receiver displays:

- **Packet rate** (packets/second)
- **Compression statistics** (compressed vs uncompressed packets)
- **Message counts** by code
- **Target message detection status**
- **Runtime duration**

## 🔍 Debugging

### Build with Debug Information
```bash
make debug
```

### Run with Verbose Output
The application automatically provides detailed statistics and status information.

### Check CSV Output
```bash
ls -la csv_output/
tail -f csv_output/message_6501_*.csv
```

## 📝 Protocol Reference

Based on **NSE CM NNF Protocol v6.3**:
- Message 6501: Table 23 (Pages 79-80)
- Structure: MS_TRADER_INT_MSG (298 bytes)
- Contains: System messages, auction notifications, margin violations, listings

## 🤝 Contributing

To add support for new NSE message codes:

1. Study the NSE protocol documentation for the message structure
2. Create new decoder files following the existing pattern
3. Implement the message parsing logic
4. Add CSV export functionality
5. Update the main dispatcher
6. Test with live data

## 📄 License

This project follows the same license as the original Go implementation.

---

**Created**: December 26, 2025  
**Converted from**: Go implementation  
**Target Performance**: 270-300 MB/s LZO decompression  
**Platform**: Linux/Unix with socket support
