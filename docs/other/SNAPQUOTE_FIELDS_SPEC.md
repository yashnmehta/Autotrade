# SnapQuote Window - Field Specifications

## Overview
The SnapQuote window displays comprehensive market data for a selected instrument including real-time quotes, market depth (bid/ask), and various trading statistics.

## Field List (Based on Reference Image and Requirements)

### **Section 1: Header Row (Scrip Selection)**
| Widget Type | Object Name | Label | Purpose | Default Value |
|------------|-------------|-------|---------|---------------|
| QComboBox | cbEx | NSE | Exchange selection | "NSE" |
| QComboBox | cbSegment | F | Segment (FO/CM/CD) | "F" |
| QLineEdit | leToken | 43756 | Token number (read-only) | "0" |
| QLineEdit | leInstType | FUTIDX | Instrument type (read-only) | "" |
| QLineEdit | leSymbol | NIFTY | Symbol name (read-only) | "" |
| QComboBox | cbExpiry | 25FEB2010 | Expiry date dropdown | "" |
| QPushButton | pbRefresh | Refresh | Refresh quote data | - |

### **Section 2: LTP (Last Traded Price) Info Box**
| Widget Type | Object Name | Label/Purpose | Display Format |
|------------|-------------|---------------|----------------|
| QLabel | lbLTPQty | Quantity | "50" |
| QLabel | lbLTPPrice | Last Traded Price | "4794.05" |
| QLabel | lbLTPIndicator | Up/Down Triangle | "▲" or "▼" |
| QLabel | lbLTPTime | Trade Time | "02:42:39 PM" |

### **Section 3: Market Statistics (Left Panel)**
| Widget Type | Object Name | Label | Display Format |
|------------|-------------|-------|----------------|
| QLabel | lbVolume | Vol.: | "22605150" |
| QLabel | lbValue | Value: | "108131508973.50" |
| QLabel | lbATP | ATP: | "4783.49" |
| QLabel | lbPercentChange | %: | "0.61" |

### **Section 4: Price Data (Right Panel - Top)**
| Widget Type | Object Name | Label | Display Format |
|------------|-------------|-------|----------------|
| QLabel | lbOpen | Open: | "4754.00" |
| QLabel | lbHigh | High: | "4818.00" |
| QLabel | lbLow | Low: | "4735.00" |
| QLabel | lbClose | Close: | "4764.90" |

### **Section 5: Additional Statistics (Right Panel - Bottom)**
| Widget Type | Object Name | Label | Display Format |
|------------|-------------|-------|----------------|
| QLabel | lbDPR | DPR: | "+4742.40 - 5796.25" |
| QLabel | lbOI | OI: | "32000050" |
| QLabel | lbOIPercent | % OI: | "2.35" |
| QLabel | lbGainLoss | Gain/Loss: | "0.00" |
| QLabel | lbMTMValue | Value: | "0.00" |
| QLabel | lbMTMPos | MTM Pos.: | "0.00" |

### **Section 6: Market Depth - Bids (Left Side, Blue Theme)**
5 levels of bid depth, each with:
| Widget Type | Object Name | Purpose | Color Scheme |
|------------|-------------|---------|--------------|
| QLabel | lbBidQty1-5 | Bid Quantity Level 1-5 | Blue (#4A90E2) |
| QLabel | lbBidPrice1-5 | Bid Price Level 1-5 | Blue (#4A90E2) |
| QLabel | lbBidOrders1-5 | Number of Orders Level 1-5 | Blue (#4A90E2) |

**Example Display**: "100 @ 4794.05"

### **Section 7: Market Depth - Asks (Right Side, Red Theme)**
5 levels of ask depth, each with:
| Widget Type | Object Name | Purpose | Color Scheme |
|------------|-------------|---------|--------------|
| QLabel | lbAskPrice1-5 | Ask Price Level 1-5 | Red (#E74C3C) |
| QLabel | lbAskQty1-5 | Ask Quantity Level 1-5 | Red (#E74C3C) |
| QLabel | lbAskOrders1-5 | Number of Orders Level 1-5 | Red (#E74C3C) |

**Example Display**: "4795.00 - 1900 - 5"

## Total Widget Count
- **7** Header widgets (4 dropdowns, 2 read-only fields, 1 button)
- **4** LTP display widgets
- **4** Market statistics (left)
- **4** Price data (right top)
- **6** Additional statistics (right bottom)
- **15** Bid depth widgets (5 levels × 3 fields each)
- **15** Ask depth widgets (5 levels × 3 fields each)

**Total: 55 widgets**

## Layout Structure

```
┌─────────────────────────────────────────────────────────────────┐
│ [NSE] [F] [43756] [FUTIDX] [NIFTY] [25FEB2010▼] [Refresh]      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐  ┌────────────────────────────────────┐   │
│  │  LTP: 50 @ ▲    │  │ Open:  4754.00    DPR: +4742-5796  │   │
│  │     4794.05     │  │ High:  4818.00    OI:  32000050    │   │
│  │  02:42:39 PM    │  │ Low:   4735.00    %OI: 2.35        │   │
│  │                 │  │ Close: 4764.90    G/L: 0.00        │   │
│  │ Vol: 22605150   │  │                   Value: 0.00      │   │
│  │ Value: 108...   │  │                   MTM: 0.00        │   │
│  │ ATP: 4783.49    │  │                                    │   │
│  │ %: 0.61         │  │                                    │   │
│  └─────────────────┘  └────────────────────────────────────┘   │
│                                                                 │
├─────────────────────────┬───────────────────────────────────────┤
│   BID DEPTH (Blue)      │      ASK DEPTH (Red)                 │
├─────────────────────────┼───────────────────────────────────────┤
│ 100      @ 4794.05      │  4795.00    1900    5                │
│ 50       @ 4793.40      │  4795.05      50    1                │
│ 1500     @ 4793.35      │  4795.60    3200    1                │
│ 50       @ 4793.10      │  4796.10     200    1                │
│ 4000     @ 4793.10      │  4796.20     200    1                │
└─────────────────────────┴───────────────────────────────────────┘
```

## Color Scheme
- **Background**: Dark (#2A3A50)
- **LTP Box**: Highlighted (#1E2A38)
- **Bid Depth**: Blue theme (#4A90E2, #3A7BC8)
- **Ask Depth**: Red theme (#E74C3C, #C0392B)
- **Text**: White/Light gray (#F0F0F0)
- **Labels**: Medium gray (#B0B0B0)

## Functionality Requirements
1. **Real-time Updates**: All price and depth fields should update in real-time
2. **Color Indicators**: 
   - Green for positive changes
   - Red for negative changes
   - Blue for bids
   - Red for asks
3. **Refresh Button**: Manual refresh trigger
4. **Read-only Fields**: Token, InstType, Symbol should not be editable
5. **Dropdown Filtering**: Expiry should filter based on selected instrument
