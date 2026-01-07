# Adding Support for New NSE CM Message Codes

## 📋 Overview

This guide provides a step-by-step process for adding support for a new NSE Capital Market (NSECM) broadcast message code to the trading terminal's UDP receiver system.

---

## 🎯 Prerequisites

Before you begin, ensure you have:

1. **Protocol Documentation**: NSE CM NNF Protocol specification (e.g., v6.3) containing:
   - Message structure definition
   - Field offsets and data types
   - Byte alignment details
   - Transaction code number

2. **Development Environment**:
   - C++20 compiler (GCC 15.2.0 or compatible)
   - CMake build system
   - Text editor with C++ support

3. **Understanding of**:
   - Big-endian to host-endian conversion
   - Structure packing (`#pragma pack`)
   - Callback pattern
   - Qt signals/slots (for UI integration)

---

## 📐 Step-by-Step Implementation Guide

### **STEP 1: Define the Transaction Code Constant**

**File**: `lib/cpp_broadcast_nsecm/include/constants.h`

**Action**: Add the new transaction code to the `TxCodes` namespace.

```cpp
namespace TxCodes {
    constexpr uint16_t BCAST_MBO_MBP_UPDATE = 7200;
    constexpr uint16_t BCAST_MW_ROUND_ROBIN = 7201;
    // ... existing codes ...
    
    // ADD YOUR NEW CODE HERE
    constexpr uint16_t BCAST_YOUR_NEW_MESSAGE = 7XXX;  // Replace with actual code
}
```

**Also update helper functions**:

1. **`isCompressed()`** - If your message is compressed:
   ```cpp
   inline bool isCompressed(uint16_t code) {
       switch(code) {
           case TxCodes::BCAST_MBO_MBP_UPDATE:
           case TxCodes::BCAST_YOUR_NEW_MESSAGE:  // Add here if compressed
           // ...
           return true;
       }
   }
   ```

2. **`getTxCodeName()`** - For debugging/logging:
   ```cpp
   inline std::string getTxCodeName(uint16_t code) {
       switch(code) {
           // ... existing cases ...
           case TxCodes::BCAST_YOUR_NEW_MESSAGE: 
               return "BCAST_YOUR_NEW_MESSAGE";
           // ...
       }
   }
   ```

---

### **STEP 2: Define the Protocol Structure**

**File**: `lib/cpp_broadcast_nsecm/include/nse_market_data.h` (or appropriate header)

**Action**: Define the C++ structure that maps to the protocol specification.

**Template**:

```cpp
#pragma pack(push, 1)  // CRITICAL: Ensure byte alignment

// ============================================================================
// BCAST_YOUR_NEW_MESSAGE (7XXX) - Brief Description
// ============================================================================

// Supporting sub-structures (if any)
struct YOUR_SUB_STRUCTURE {
    int32_t field1;                  // Offset 0 (4 bytes)
    int16_t field2;                  // Offset 4 (2 bytes)
    char field3[10];                 // Offset 6 (10 bytes)
    // ... document all fields with offsets
};

// Main message structure
// Protocol Reference: NSE CM NNF Protocol v6.3, Page XXX
// Total Size: XXX bytes
struct MS_BCAST_YOUR_NEW_MESSAGE {
    BCAST_HEADER header;             // Offset 0 (40 bytes) - Always present
    
    // Your message-specific fields
    uint32_t token;                  // Offset 40 (4 bytes)
    int16_t fieldA;                  // Offset 44 (2 bytes)
    int32_t fieldB;                  // Offset 46 (4 bytes)
    int64_t fieldC;                  // Offset 50 (8 bytes)
    YOUR_SUB_STRUCTURE subData;      // Offset 58 (16 bytes)
    
    // IMPORTANT: Document exact byte offsets from protocol spec
    // IMPORTANT: Use exact data types (int16_t, int32_t, int64_t, char[])
    // IMPORTANT: Match the protocol's endianness (usually big-endian)
};

#pragma pack(pop)  // Restore default packing
```

**Key Points**:
- ✅ Always start with `BCAST_HEADER` (40 bytes)
- ✅ Use exact sizes: `int16_t` (2 bytes), `int32_t` (4 bytes), `int64_t` (8 bytes)
- ✅ Document byte offsets in comments
- ✅ Use `#pragma pack(push, 1)` for exact byte alignment
- ✅ NSE uses **big-endian** encoding - you'll convert in the parser

---

### **STEP 3: Define Parsed Data Structures**

**File**: `lib/cpp_broadcast_nsecm/include/nsecm_callback.h`

**Action**: Create user-friendly structures for the parsed data.

```cpp
namespace nsecm {

// ============================================================================
// PARSED DATA STRUCTURES FOR CALLBACKS
// ============================================================================

// Parsed data for your new message type
struct YourNewMessageData {
    uint32_t token;
    double price;              // Converted from paise to rupees
    uint64_t volume;
    int32_t someValue;
    char description[25];
    
    // Latency tracking (optional but recommended)
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// ============================================================================
// CALLBACK FUNCTION TYPES
// ============================================================================

// Callback type definition
using YourNewMessageCallback = std::function<void(const YourNewMessageData&)>;

} // namespace nsecm
```

**Also update the `MarketDataCallbackRegistry` class**:

```cpp
class MarketDataCallbackRegistry {
public:
    // ... existing methods ...
    
    // Register callback for your new message type
    void registerYourNewMessageCallback(YourNewMessageCallback callback) {
        yourNewMessageCallback = callback;
    }
    
    // Dispatch callback (called by parser)
    void dispatchYourNewMessage(const YourNewMessageData& data) {
        if (yourNewMessageCallback) {
            yourNewMessageCallback(data);
        }
    }
    
private:
    // ... existing callbacks ...
    YourNewMessageCallback yourNewMessageCallback;
};
```

---

### **STEP 4: Declare the Parser Function**

**File**: `lib/cpp_broadcast_nsecm/include/nse_parsers.h`

**Action**: Declare the parser function signature.

```cpp
namespace nsecm {

// ============================================================================
// MARKET DATA MESSAGE PARSERS
// ============================================================================

// ... existing parser declarations ...

// TransCode 7XXX
void parse_message_7XXX(const MS_BCAST_YOUR_NEW_MESSAGE* msg);
void parse_your_new_message(const MS_BCAST_YOUR_NEW_MESSAGE* msg);  // Alias

} // namespace nsecm
```

---

### **STEP 5: Implement the Parser**

**File**: `lib/cpp_broadcast_nsecm/src/parser/parse_message_7XXX.cpp` (create new file)

**Template**:

```cpp
#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_7XXX(const MS_BCAST_YOUR_NEW_MESSAGE* msg) {
    // STEP 1: Extract basic identification
    uint32_t token = be32toh_func(msg->token);  // Convert big-endian to host
    
    // STEP 2: Capture timestamps for latency tracking
    static uint64_t refNoCounter = 0;
    uint64_t refNo = ++refNoCounter;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // STEP 3: Parse and convert all fields
    YourNewMessageData data;
    data.token = token;
    data.refNo = refNo;
    data.timestampRecv = now;
    data.timestampParsed = now;
    
    // Convert numeric fields (big-endian to host-endian)
    data.someValue = be32toh_func(msg->fieldB);
    data.volume = be64toh_func(msg->fieldC);
    
    // Convert price from paise to rupees (if applicable)
    int32_t priceInPaise = be32toh_func(msg->priceField);
    data.price = static_cast<double>(priceInPaise) / 100.0;
    
    // Copy string fields (ensure null-termination)
    size_t copySize = sizeof(data.description) - 1;
    if (sizeof(msg->descriptionField) < copySize) {
        copySize = sizeof(msg->descriptionField);
    }
    std::memcpy(data.description, msg->descriptionField, copySize);
    data.description[sizeof(data.description) - 1] = '\0';
    
    // STEP 4: Validate data (optional but recommended)
    if (token == 0) {
        std::cerr << "[parse_message_7XXX] Invalid token: 0" << std::endl;
        return;
    }
    
    // STEP 5: Dispatch callback
    MarketDataCallbackRegistry::instance().dispatchYourNewMessage(data);
}

// Alias function
void parse_your_new_message(const MS_BCAST_YOUR_NEW_MESSAGE* msg) {
    parse_message_7XXX(msg);
}

} // namespace nsecm
```

**Key Conversion Functions**:
- `be16toh_func()` - For 16-bit (short) fields
- `be32toh_func()` - For 32-bit (long/int) fields
- `be64toh_func()` - For 64-bit (long long) fields
- Divide by 100.0 - Convert paise to rupees
- `std::memcpy()` - Copy fixed-size character arrays safely

---

### **STEP 6: Add Routing in Message Dispatcher**

**File**: `lib/cpp_broadcast_nsecm/src/utils/parse_compressed_message.cpp` (for compressed)
**OR**: `lib/cpp_broadcast_nsecm/src/utils/parse_uncompressed_message.cpp` (for uncompressed)

**Action**: Add a case to route your message type to the parser.

**For Compressed Messages**:

```cpp
void parse_compressed_message(const char* data, int16_t length, UDPStats& stats) {
    // ... existing decompression code ...
    
    // Extract Transaction Code
    uint16_t txCode = be16toh_func(*((uint16_t*)(message_data + 10)));
    
    // Route by transaction code
    switch (txCode) {
        case TxCodes::BCAST_MBO_MBP_UPDATE:
            // ... existing case ...
            break;
            
        // ADD YOUR NEW MESSAGE HERE
        case TxCodes::BCAST_YOUR_NEW_MESSAGE:
            if (message_size >= sizeof(MS_BCAST_YOUR_NEW_MESSAGE)) {
                parse_your_new_message(
                    reinterpret_cast<const MS_BCAST_YOUR_NEW_MESSAGE*>(message_data)
                );
            } else {
                std::cerr << "[7XXX] Message too small: " << message_size 
                          << " (expected " << sizeof(MS_BCAST_YOUR_NEW_MESSAGE) << ")" 
                          << std::endl;
            }
            break;
            
        default:
            break;
    }
}
```

**For Uncompressed Messages**:

```cpp
void parse_uncompressed_message(const char* data, int16_t length) {
    // ... existing code ...
    
    switch (txCode) {
        // ... existing cases ...
        
        case TxCodes::BCAST_YOUR_NEW_MESSAGE:
            if (length >= sizeof(MS_BCAST_YOUR_NEW_MESSAGE)) {
                parse_your_new_message(
                    reinterpret_cast<const MS_BCAST_YOUR_NEW_MESSAGE*>(data)
                );
            }
            break;
            
        default:
            break;
    }
}
```

---

### **STEP 7: Register Callback in Service Layer** (Qt Integration)

**File**: `src/services/UdpBroadcastService.cpp`

**Action**: Register a callback in the `setupNseCmCallbacks()` method.

```cpp
void UdpBroadcastService::setupNseCmCallbacks() {
    // ... existing callbacks ...
    
    // Register callback for your new message type
    nsecm::MarketDataCallbackRegistry::instance().registerYourNewMessageCallback(
        [this](const nsecm::YourNewMessageData& data) {
            // Option 1: Convert to existing UDP::MarketTick (if applicable)
            UDP::MarketTick udpTick(UDP::ExchangeSegment::NSECM, data.token);
            udpTick.ltp = data.price;
            udpTick.volume = data.volume;
            udpTick.refNo = data.refNo;
            udpTick.timestampUdpRecv = data.timestampRecv;
            udpTick.timestampParsed = data.timestampParsed;
            udpTick.timestampEmitted = LatencyTracker::now();
            udpTick.messageType = 7XXX;
            emit udpTickReceived(udpTick);
            
            // Option 2: Create a new signal type (if data doesn't fit existing types)
            // emit yourNewMessageReceived(data);
            
            // Legacy compatibility (if needed)
            XTS::Tick legacyTick = convertToLegacy(udpTick);
            m_totalTicks++;
            emit tickReceived(legacyTick);
        });
}
```

**If you need a new signal type**, add to `UdpBroadcastService.h`:

```cpp
signals:
    void tickReceived(const XTS::Tick& tick);
    void udpTickReceived(const UDP::MarketTick& tick);
    void yourNewMessageReceived(const YourNewMessageData& data);  // NEW
```

---

### **STEP 8: Update CMakeLists.txt**

**File**: `lib/cpp_broadcast_nsecm/CMakeLists.txt` (or main CMakeLists.txt)

**Action**: Add the new parser source file to the build.

```cmake
set(NSECM_SOURCES
    src/multicast_receiver.cpp
    src/parser/parse_message_7200.cpp
    src/parser/parse_message_7208.cpp
    src/parser/parse_message_7XXX.cpp  # ADD YOUR NEW FILE HERE
    # ... other sources ...
)
```

---

### **STEP 9: Build and Test**

#### **9.1 Compile the Code**

```bash
cd build
cmake ..
cmake --build . --config Release
```

**Fix any compilation errors**:
- Missing includes
- Typos in structure names
- Incorrect data types

#### **9.2 Test with Live Data**

1. **Enable logging** (temporarily add debug output):
   ```cpp
   void parse_message_7XXX(const MS_BCAST_YOUR_NEW_MESSAGE* msg) {
       std::cout << "[7XXX] Token: " << token 
                 << " | Price: " << data.price 
                 << " | Volume: " << data.volume << std::endl;
       // ...
   }
   ```

2. **Run the application** and verify console output when your message arrives.

3. **Check for errors**:
   - Wrong field offsets → garbage data
   - Missing endianness conversion → very large/small numbers
   - Incorrect size → parsing crashes or truncated data

#### **9.3 Test with Captured Data**

Create a standalone test program (optional):

```cpp
// test_message_7XXX.cpp
#include "nse_parsers.h"
#include "protocol.h"
#include <fstream>
#include <vector>

int main() {
    // Read captured binary message from file
    std::ifstream file("message_7XXX_sample.bin", std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), 
                              std::istreambuf_iterator<char>());
    
    if (data.size() >= sizeof(nsecm::MS_BCAST_YOUR_NEW_MESSAGE)) {
        const nsecm::MS_BCAST_YOUR_NEW_MESSAGE* msg = 
            reinterpret_cast<const nsecm::MS_BCAST_YOUR_NEW_MESSAGE*>(data.data());
        
        nsecm::parse_message_7XXX(msg);
        std::cout << "Test passed!" << std::endl;
    } else {
        std::cerr << "File too small" << std::endl;
    }
    
    return 0;
}
```

---

## 🔍 Debugging Checklist

### **Common Issues and Solutions**

| **Issue** | **Cause** | **Solution** |
|-----------|-----------|--------------|
| **Garbage data in fields** | Wrong byte offset or structure padding | Verify `#pragma pack(push, 1)` and check protocol doc for exact offsets |
| **Very large numbers** | Missing endianness conversion | Use `be16toh_func()`, `be32toh_func()`, `be64toh_func()` |
| **Segmentation fault** | Structure size mismatch | Check `sizeof(MS_BCAST_YOUR_NEW_MESSAGE)` matches protocol spec |
| **Message not parsed** | Missing routing case | Check `parse_compressed_message.cpp` has your `TxCodes::BCAST_YOUR_NEW_MESSAGE` case |
| **Callback not fired** | Not registered in service | Verify `registerYourNewMessageCallback()` is called in `setupNseCmCallbacks()` |
| **Prices too large/small** | Missing paise→rupees conversion | Divide price fields by 100.0 |

---

## 📊 Example: Adding Message 7210 (Security Status Change)

Let's walk through a complete example:

### **1. Add constant** (`constants.h`):
```cpp
constexpr uint16_t BCAST_SECURITY_STATUS_CHG_PREOPEN = 7210;
```

### **2. Define structure** (`nse_market_data.h`):
```cpp
#pragma pack(push, 1)
struct MS_BCAST_SECURITY_STATUS_CHG_PREOPEN {
    BCAST_HEADER header;        // 40 bytes
    uint32_t token;             // 4 bytes
    uint16_t securityStatus;    // 2 bytes
    uint16_t reserved;          // 2 bytes
};
#pragma pack(pop)
```

### **3. Define parsed data** (`nsecm_callback.h`):
```cpp
struct SecurityStatusData {
    uint32_t token;
    uint16_t securityStatus;
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
};

using SecurityStatusCallback = std::function<void(const SecurityStatusData&)>;
```

### **4. Implement parser** (`parse_message_7210.cpp`):
```cpp
void parse_message_7210(const MS_BCAST_SECURITY_STATUS_CHG_PREOPEN* msg) {
    SecurityStatusData data;
    data.token = be32toh_func(msg->token);
    data.securityStatus = be16toh_func(msg->securityStatus);
    data.refNo = ++refNoCounter;
    data.timestampRecv = LatencyTracker::now();
    
    MarketDataCallbackRegistry::instance().dispatchSecurityStatus(data);
}
```

### **5. Add routing** (`parse_compressed_message.cpp`):
```cpp
case TxCodes::BCAST_SECURITY_STATUS_CHG_PREOPEN:
    if (message_size >= sizeof(MS_BCAST_SECURITY_STATUS_CHG_PREOPEN)) {
        parse_message_7210(reinterpret_cast<const MS_BCAST_SECURITY_STATUS_CHG_PREOPEN*>(message_data));
    }
    break;
```

### **6. Register callback** (`UdpBroadcastService.cpp`):
```cpp
nsecm::MarketDataCallbackRegistry::instance().registerSecurityStatusCallback(
    [this](const nsecm::SecurityStatusData& data) {
        emit securityStatusChanged(data.token, data.securityStatus);
    });
```

Done! ✅

---

## 📚 Reference Documentation

### **Key Files Reference**

| **File** | **Purpose** | **What to Add** |
|----------|-------------|-----------------|
| `constants.h` | Transaction code definitions | New `constexpr uint16_t` constant |
| `nse_market_data.h` | Protocol structures | New message structure |
| `nsecm_callback.h` | Parsed data & callbacks | Parsed data struct, callback type, registry methods |
| `nse_parsers.h` | Parser declarations | Function declaration |
| `parse_message_XXXX.cpp` | Parser implementation | Parsing logic |
| `parse_compressed_message.cpp` | Message routing | New `case` in switch statement |
| `UdpBroadcastService.cpp` | Qt integration | Callback registration |

### **Data Type Mapping**

| **Protocol Type** | **C++ Type** | **Endianness** | **Conversion** |
|-------------------|--------------|----------------|----------------|
| SHORT | `int16_t` | Big-endian | `be16toh_func()` |
| LONG | `int32_t` | Big-endian | `be32toh_func()` |
| LONG LONG | `int64_t` | Big-endian | `be64toh_func()` |
| CHAR[N] | `char[N]` | - | `std::memcpy()` |
| Price (paise) | `int32_t` → `double` | Big-endian | `be32toh_func() / 100.0` |

---

## ✅ Final Checklist

Before committing your code:

- [ ] Transaction code added to `constants.h`
- [ ] Protocol structure defined with `#pragma pack(push, 1)`
- [ ] Parsed data structure created
- [ ] Callback type and registry methods added
- [ ] Parser function declared in `nse_parsers.h`
- [ ] Parser implemented with proper endianness conversion
- [ ] Routing case added to dispatcher
- [ ] Callback registered in `UdpBroadcastService`
- [ ] CMakeLists.txt updated (if new file created)
- [ ] Code compiles without errors
- [ ] Tested with live or captured data
- [ ] Debug logging removed (or conditional)
- [ ] Code documented with comments

---

## 🚀 Advanced Topics

### **Handling Multiple Records**

Some messages contain arrays of records:

```cpp
struct MS_BCAST_MULTIPLE_RECORDS {
    BCAST_HEADER header;
    uint16_t noOfRecords;
    RECORD_TYPE records[28];  // Maximum 28 records
};

void parse_multiple_records(const MS_BCAST_MULTIPLE_RECORDS* msg) {
    uint16_t count = be16toh_func(msg->noOfRecords);
    
    for (uint16_t i = 0; i < count && i < 28; i++) {
        // Parse each record
        const RECORD_TYPE& rec = msg->records[i];
        // ... process record ...
    }
}
```

### **Handling Variable-Length Messages**

If message size varies:

```cpp
// In routing logic
if (message_size >= sizeof(BCAST_HEADER)) {
    uint16_t recordCount = be16toh_func(*((uint16_t*)(message_data + 40)));
    size_t expectedSize = sizeof(BCAST_HEADER) + 2 + (recordCount * sizeof(RECORD_TYPE));
    
    if (message_size >= expectedSize) {
        parse_variable_message(...);
    }
}
```

### **Performance Optimization**

1. **Pre-allocate buffers**: Use stack allocation where possible
2. **Minimize copies**: Use references and pointers
3. **Cache frequently accessed data**: Store parsed tokens in maps
4. **Profile hotspots**: Use profiling tools to identify bottlenecks

---

## 📞 Support

For questions or issues:
1. Check the NSE protocol documentation first
2. Review existing parser implementations (e.g., `parse_message_7200.cpp`)
3. Enable debug logging to trace data flow
4. Verify structure sizes with `sizeof()` against protocol spec

---

**Last Updated**: January 7, 2026
**Version**: 1.0
**Author**: Trading Terminal Development Team
