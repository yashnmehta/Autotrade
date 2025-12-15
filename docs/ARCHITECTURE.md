# Trading Terminal C++ Architecture

## Overview
This document outlines the architectural decisions for the Trading Terminal application, emphasizing clear separation between stable custom UI widgets and evolving trading business logic.

## Design Principles

### 1. **Separation of Concerns**
- **STABLE FRAMEWORK**: Custom Qt widgets that are tested and rarely change
- **ACTIVE DEVELOPMENT**: Trading logic, API integration, and business rules

### 2. **AI-Friendly Development**
- Clear boundaries prevent AI from modifying stable, tested widgets
- Modular structure makes it easy to focus on specific features
- Explicit naming conventions guide development

### 3. **Modularity**
- Each class has a single responsibility
- Utility functions are organized by domain
- Large files are split into logical components

## Directory Structure

```
trading_terminal_cpp/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   │
│   ├── core/                    ← STABLE FRAMEWORK (⚠️ rarely changes)
│   │   ├── widgets/             ← Custom Qt widgets (TESTED, DON'T TOUCH)
│   │   │   ├── CustomMainWindow.cpp/.h
│   │   │   ├── CustomMDIArea.cpp/.h
│   │   │   ├── CustomMDIChild.cpp/.h
│   │   │   ├── CustomMDISubWindow.cpp/.h
│   │   │   ├── CustomTitleBar.cpp/.h
│   │   │   ├── CustomScripComboBox.cpp/.h
│   │   │   ├── MDITaskBar.cpp/.h
│   │   │   ├── InfoBar.cpp/.h
│   │   │   ├── CustomMarketWatch.cpp/.h      ← Base class for market watch views
│   │   │   └── CustomNetPosition.cpp/.h      ← Base class for position views
│   │   └── base/                ← Base classes & interfaces
│   │       └── BaseTradingWindow.h           ← Common interface for all trading windows
│   │
│   ├── app/                     ← APPLICATION LOGIC (✅ actively developed)
│   │   ├── Application.cpp/.h   ← Main app orchestrator (like Go's app.go)
│   │   ├── MainWindow.cpp/.h    ← Main application window
│   │   └── ScripBar.cpp/.h      ← Scrip search bar (app-level)
│   │
│   ├── views/                   ← TRADING WINDOWS (✅ actively developed)
│   │   ├── BuyWindow.cpp/.h
│   │   ├── SellWindow.cpp/.h
│   │   ├── MarketWatchWindow.cpp/.h
│   │   ├── SnapQuoteWindow.cpp/.h
│   │   ├── PositionWindow.cpp/.h
│   │   ├── OrderBookWindow.cpp/.h      ← Future
│   │   ├── TradeBookWindow.cpp/.h      ← Future
│   │   └── OptionChainWindow.cpp/.h    ← Future
│   │
│   ├── models/                  ← DATA MODELS (✅ actively developed)
│   │   ├── MarketWatchModel.cpp/.h
│   │   ├── PositionModel.cpp/.h
│   │   ├── OrderModel.cpp/.h           ← Future
│   │   ├── TradeModel.cpp/.h           ← Future
│   │   └── TokenAddressBook.cpp/.h
│   │
│   ├── services/                ← BUSINESS LOGIC (✅ actively developed, no Qt dependency ideally)
│   │   ├── xts/
│   │   │   ├── XtsClient.cpp/.h        ← HTTP API client
│   │   │   ├── XtsWebSocket.cpp/.h     ← WebSocket handler
│   │   │   └── XtsConstants.h
│   │   ├── FeedManager.cpp/.h          ← Market data distribution
│   │   ├── OrderManager.cpp/.h         ← Order placement/tracking
│   │   └── ScripSearchService.cpp/.h   ← Search logic
│   │
│   ├── repository/              ← DATA ACCESS (✅ actively developed)
│   │   ├── ScripMaster.cpp/.h          ← Contract data loading
│   │   ├── Greeks.cpp/.h               ← Greeks calculations
│   │   ├── ConfigManager.cpp/.h        ← Settings
│   │   └── WorkspaceManager.cpp/.h     ← Window layouts
│   │
│   └── utils/                   ← UTILITIES (✅ actively developed)
│       ├── Formatters.cpp/.h           ← Price/number formatting
│       ├── Validators.cpp/.h           ← Input validation
│       └── Constants.h                 ← App-wide constants
│
├── include/                     ← PUBLIC HEADERS (mirrors src structure)
│   ├── core/
│   │   ├── widgets/
│   │   └── base/
│   ├── app/
│   ├── views/
│   ├── models/
│   ├── services/
│   ├── repository/
│   └── utils/
│
├── resources/
│   ├── forms/                   ← .ui files
│   ├── icons/
│   └── resources.qrc
│
├── config/
│   └── config.ini
│
├── docs/                        ← Documentation
│   ├── ARCHITECTURE.md          ← This file
│   ├── WIDGETS_CUSTOMIZATION.md ← Custom widget details
│   └── API_INTEGRATION.md       ← XTS API integration guide
│
└── tests/
    ├── core/                    ← Widget tests
    ├── services/                ← Service tests
    └── views/                   ← View tests
```

## Component Descriptions

### Core Layer (Stable Framework)

#### `core/widgets/` - Custom Qt Widgets
**Status**: ⚠️ **STABLE - Rarely Modified**

These are fully tested, production-ready custom widgets:

1. **CustomMainWindow**: Main window with macOS native menu bar integration
2. **CustomMDIArea**: MDI area with window management
3. **CustomMDIChild**: MDI child window wrapper
4. **CustomMDISubWindow**: Subwindow with custom title bar
5. **CustomTitleBar**: Custom title bar with window controls
6. **CustomScripComboBox**: Specialized combo box for scrip selection
7. **MDITaskBar**: Task bar for window switching
8. **InfoBar**: Information/status bar component
9. **CustomMarketWatch**: Base class for market watch table views (inherits QTableView)
10. **CustomNetPosition**: Base class for position table views (inherits QTableView)

**Rules**:
- ❌ AI should NOT modify these unless explicitly requested
- ✅ Can be subclassed in `views/` for specific use cases
- ✅ New stable widgets added here after thorough testing

#### `core/base/` - Base Classes
Abstract base classes and interfaces that define contracts:

- **BaseTradingWindow**: Common interface for all trading windows (buy, sell, positions, etc.)

### Application Layer

#### `app/` - Application Logic
**Status**: ✅ **Active Development**

1. **Application**: Main application orchestrator
   - Initializes services
   - Manages application lifecycle
   - Coordinates between components

2. **MainWindow**: Main application window
   - Menu bar and actions
   - MDI window management
   - Workspace management

3. **ScripBar**: Global scrip search bar
   - Quick scrip search
   - Recent searches

### Views Layer

#### `views/` - Trading Windows
**Status**: ✅ **Active Development**

Each view is responsible for:
- User interactions
- Display logic
- Connecting to models and services

**Current Views**:
- **BuyWindow**: Order entry for buy orders
- **SellWindow**: Order entry for sell orders
- **MarketWatchWindow**: Real-time market data display (subclasses CustomMarketWatch)
- **SnapQuoteWindow**: Quick quote view for a single scrip
- **PositionWindow**: Position tracking and P&L (subclasses CustomNetPosition)

**Future Views**:
- **OrderBookWindow**: Order management
- **TradeBookWindow**: Trade history
- **OptionChainWindow**: Options chain display

**Best Practices**:
- Subclass `CustomMarketWatch` for table-based market data views
- Subclass `CustomNetPosition` for position/P&L table views
- Keep view logic separate from business logic
- Use signals/slots for communication

### Models Layer

#### `models/` - Data Models
**Status**: ✅ **Active Development**

Qt models that bridge data and views:

1. **MarketWatchModel**: Model for market watch table
   - Manages scrip list
   - Updates market data
   - Handles token subscriptions

2. **PositionModel**: Model for position table
   - Position data
   - P&L calculations
   - Summary rows

3. **TokenAddressBook**: Token to model row mapping
   - Fast token lookups
   - Row index management

**Future Models**:
- **OrderModel**: Order book data
- **TradeModel**: Trade history data

### Services Layer

#### `services/` - Business Logic
**Status**: ✅ **Active Development** (Future Work)

Pure business logic with minimal Qt dependencies:

1. **XTS Integration** (`services/xts/`)
   - **XtsClient**: REST API client for XTS
   - **XtsWebSocket**: WebSocket connection for market data
   - **XtsConstants**: API constants and endpoints

2. **FeedManager**: Market data distribution
   - Subscribes to tokens
   - Distributes ticks to subscribers
   - Manages WebSocket connections

3. **OrderManager**: Order lifecycle management
   - Order placement
   - Order modification/cancellation
   - Order status updates

4. **ScripSearchService**: Scrip search logic
   - Search algorithms
   - Filtering and ranking

### Repository Layer

#### `repository/` - Data Access
**Status**: ✅ **Active Development**

Data access and persistence:

1. **ScripMaster**: Contract master data
   - Load scrip data from CSV/database
   - Search and filter scrips

2. **Greeks**: Options Greeks calculations
   - Black-Scholes calculations
   - Greeks updates

3. **ConfigManager**: Application settings
   - Load/save configuration
   - User preferences

4. **WorkspaceManager**: Window layouts
   - Save/restore window positions
   - Workspace templates

### Utils Layer

#### `utils/` - Utilities
**Status**: ✅ **Active Development**

Reusable utility functions:

- **Formatters**: Price, number, date formatting
- **Validators**: Input validation helpers
- **Constants**: Application-wide constants

## Code Organization Guidelines

### Modularity Rules

1. **Single Responsibility**: Each class should have one clear purpose
2. **File Size Limit**: Keep files under 500 lines; split if larger
3. **Utility Functions**: 
   - Class-specific utilities → private methods or nested namespace in .cpp
   - Shared utilities → `utils/` directory
   - Example:
     ```cpp
     // In PositionWindow.cpp
     namespace {
         double calculatePnL(double buyPrice, double sellPrice, int qty) {
             return (sellPrice - buyPrice) * qty;
         }
     }
     ```

4. **Separation of Concerns**:
   - Views handle UI only
   - Models handle data only
   - Services handle business logic only

### Naming Conventions

- **Stable Widgets**: Prefix with `Custom` (e.g., `CustomMDIArea`)
- **Base Classes**: Prefix with `Base` (e.g., `BaseTradingWindow`)
- **Views**: Suffix with `Window` (e.g., `BuyWindow`)
- **Models**: Suffix with `Model` (e.g., `MarketWatchModel`)
- **Services**: Suffix with `Manager` or `Service` (e.g., `FeedManager`, `ScripSearchService`)

### Include Path Convention

```cpp
// Core widgets (stable)
#include "core/widgets/CustomMDIArea.h"

// Application
#include "app/MainWindow.h"

// Views
#include "views/MarketWatchWindow.h"

// Models
#include "models/MarketWatchModel.h"

// Services
#include "services/FeedManager.h"
#include "services/xts/XtsClient.h"

// Repository
#include "repository/ScripMaster.h"

// Utils
#include "utils/Formatters.h"
```

## Customizations Made

### Custom Qt Widgets Overview

Our custom widgets provide a polished, professional trading terminal UI:

1. **CustomMainWindow**
   - Native macOS menu bar integration
   - MDI area management
   - Window state persistence

2. **CustomMDIArea**
   - Tabbed interface support
   - Window activation tracking
   - Drag-and-drop window management

3. **CustomMDISubWindow**
   - Custom title bar integration
   - Resize handles
   - Window controls (minimize, maximize, close)

4. **CustomTitleBar**
   - Native macOS-style buttons
   - Double-click to maximize
   - Drag to move window

5. **CustomScripComboBox**
   - Optimized for large scrip lists
   - Search and filter
   - Recent selections

6. **MDITaskBar**
   - Window switching via buttons
   - Active window highlighting

7. **InfoBar**
   - Connection status
   - User info display
   - Market status

8. **CustomMarketWatch** (Base Class)
   - Pre-configured table view
   - Column management
   - Sorting and filtering base
   - Ready for market data display

9. **CustomNetPosition** (Base Class)
   - Pre-configured position table
   - P&L display formatting
   - Summary row support
   - Filtering infrastructure

For detailed customization information, see [WIDGETS_CUSTOMIZATION.md](WIDGETS_CUSTOMIZATION.md).

## Development Workflow

### Adding New Features

1. **Identify Layer**: Determine which layer the feature belongs to
2. **Create Files**: Place files in appropriate directory
3. **Follow Patterns**: Use existing code as reference
4. **Update CMakeLists.txt**: Add new source files
5. **Test**: Build and verify

### Modifying Existing Code

1. **Core Widgets**: ⚠️ Avoid unless absolutely necessary
2. **Views/Models/Services**: ✅ Actively develop here
3. **Test Impact**: Verify changes don't break existing features

### AI Development Guidelines

When working with AI:
- ✅ Focus AI on `views/`, `models/`, `services/`, `repository/`, `utils/`
- ❌ Instruct AI to avoid `core/widgets/` unless explicitly needed
- ✅ Use clear prompts: "Create a new service in services/" vs "Fix the widget"
- ✅ Reference architecture: "Follow the architecture in docs/ARCHITECTURE.md"

## Future Integration

### XTS API Integration

See [API_INTEGRATION.md](API_INTEGRATION.md) for details on:
- Authentication flow
- WebSocket market data
- Order placement API
- Error handling

### Python/Go Reference

- **Go Implementation** (`bettle` app): Reference for API integration patterns
- **Python Implementation**: Reference for data processing and algorithms

## Migration from Current Structure

### Step-by-Step Migration

1. ✅ Create new directory structure
2. ✅ Move files to appropriate directories
3. ✅ Update CMakeLists.txt
4. ✅ Update all `#include` paths
5. ✅ Build and verify
6. ✅ Remove dummy data
7. ✅ Test all existing features

### Dummy Data Removal

Current dummy data locations:
- **MarketWatchWindow**: Hardcoded scrips (NIFTY 50, BANKNIFTY, etc.)
- **PositionWindow**: Hardcoded positions
- **BuyWindow/SellWindow**: Default values

Action:
- Replace with empty states
- Add "No data" placeholders
- Prepare for real data integration

## Testing Strategy

1. **Unit Tests**: Test individual components in `tests/`
2. **Integration Tests**: Test component interactions
3. **UI Tests**: Manual testing of widgets
4. **Regression Tests**: Ensure existing features work after changes

## Version Control

- **QTshell_v0**: Stable baseline with all custom widgets
- **refactor**: Active development branch with new structure
- **Feature branches**: Branch from `refactor` for new features

## Questions & Decisions

### Open Questions
1. Should we use Qt's Model/View architecture more extensively?
2. How to handle real-time data updates efficiently?
3. Threading strategy for WebSocket and API calls?

### Decisions Made
1. ✅ Separate stable widgets from active development
2. ✅ Use subclassing for CustomMarketWatch and CustomNetPosition
3. ✅ Place business logic in services/ with minimal Qt dependency
4. ✅ Use repository pattern for data access

## References

- [Qt Model/View Programming](https://doc.qt.io/qt-5/model-view-programming.html)
- [Qt Widgets](https://doc.qt.io/qt-5/qtwidgets-index.html)
- XTS API Documentation
- Go Bettle Application (reference implementation)
- Python Trading Scripts (reference algorithms)

---

**Document Version**: 1.0  
**Last Updated**: December 15, 2025  
**Maintainer**: Trading Terminal Development Team
