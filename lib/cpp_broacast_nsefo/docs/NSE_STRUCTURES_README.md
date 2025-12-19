# NSE Protocol Structures - Organization

## File Structure

The NSE protocol structures have been organized into 4 logical header files for better maintainability:

### 1. `nse_common.h`
**Purpose:** Common/base structures used across all message types

**Contains:**
- `BCAST_HEADER` (40 bytes) - Common header for all broadcast messages
- `ST_INDICATOR` (2 bytes) - Market condition indicators
- `ST_MBO_MBP_TERMS` (2 bytes) - Terms for MBO/MBP messages

**Usage:** Automatically included by other NSE headers

---

### 2. `nse_market_data.h`
**Purpose:** Real-time market data broadcast messages (mostly compressed)

**Contains:**
- **MS_BCAST_MBO_MBP** (7200) - Market By Order/Price Update - 410 bytes
- **MS_BCAST_ONLY_MBP** (7208) - Market By Price Only - 470 bytes
- **MS_TICKER_TRADE_DATA** (7202) - Ticker and Market Index - 484 bytes
- **MS_ENHNCD_TICKER_TRADE_DATA** (17202) - Enhanced Ticker - 498 bytes

**Supporting Structures:**
- `ST_MBO_INFO`, `ST_MBP_INFO`, `ST_INTERACTIVE_MBO_DATA`
- `INTERACTIVE_ONLY_MBP_DATA`, `MBP_INFORMATION`
- `ST_TICKER_INDEX_INFO`, `ST_ENHNCD_TICKER_INDEX_INFO`

**Usage:** Include when parsing market data broadcasts

---

### 3. `nse_admin_messages.h`
**Purpose:** Administrative and control messages (mostly uncompressed)

**Contains:**
- **MS_BC_CIRCUIT_CHECK** (6541) - Circuit breaker check - 40 bytes
- **MS_BC_OPEN_MSG** (6511) - Market open - 40 bytes
- **MS_BC_CLOSE_MSG** (6521) - Market close - 40 bytes
- **MS_BC_POSTCLOSE_MSG** (6522) - Post-close - 40 bytes
- **MS_BC_PRE_OR_POST_DAY_MSG** (6531) - Pre/post day - 40 bytes
- **MS_BC_NORMAL_MKT_PREOPEN_ENDED** (6571) - Pre-open ended - 40 bytes

**Usage:** Include when handling market status and control messages

---

### 4. `nse_database_updates.h`
**Purpose:** Database and master data update messages

**Contains:** (Placeholders for now)
- Local database updates (7304, 7307, 7308)
- Security master changes (7305, 7340)
- Participant master changes (7306)
- Spread master changes (7309, 7341)
- Index updates (7325, 7326)
- Market statistics (1833, 11833)

**Usage:** Include when handling database synchronization

---

### 5. `nse_structures.h` (Master Header)
**Purpose:** Convenience header that includes all NSE structure files

**Usage:**
```cpp
#include "nse_structures.h"  // Includes everything
```

Or include specific headers:
```cpp
#include "nse_common.h"       // Just common structures
#include "nse_market_data.h"  // Just market data structures
```

---

## Benefits of This Organization

✅ **Better Organization** - Logical grouping by message type
✅ **Faster Compilation** - Include only what you need
✅ **Easier Maintenance** - Changes isolated to specific files
✅ **Clear Dependencies** - Common structures separated from specific ones
✅ **Scalability** - Easy to add new structures to appropriate files

---

## Current Status

**Completed Structures:** 8/47 (17%)
- ✅ All common structures
- ✅ 3 market data messages (7200, 7208, 7202, 17202)
- ✅ 6 admin messages (6511, 6521, 6522, 6531, 6541, 6571)

**Pending:**
- Market data: 7201, 7211, 7220, 17201
- Database updates: All structures (7304-7341, 1833, 11833, etc.)
- Additional admin messages: 6501, 5295, 6013, 9010, 9011, etc.

---

## Adding New Structures

When adding new structures:

1. Determine the category (market data, admin, or database)
2. Add to the appropriate header file
3. Include all nested structures
4. Verify byte offsets match protocol document
5. Add transaction code comment
6. Use `#pragma pack(push, 1)` for byte alignment

Example:
```cpp
// MS_NEW_MESSAGE - XXX bytes
// Transaction Code: XXXX
struct MS_NEW_MESSAGE {
    BCAST_HEADER header;    // Offset 0 (40 bytes)
    // ... additional fields
};
```
