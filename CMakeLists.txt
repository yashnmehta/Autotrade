cmake_minimum_required(VERSION 3.14)

project(TradingTerminal LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set AUTOUIC search paths for .ui files
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/resources/forms)

# Find Qt5 components (UI only)
find_package(Qt5 COMPONENTS Widgets Network WebSockets UiTools REQUIRED)

# Find Boost for native WebSocket (Beast + Asio)
# Boost.Beast is header-only, just need Boost headers
find_package(Boost 1.70 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Include directories - New Structured Architecture
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/core/widgets
    ${CMAKE_SOURCE_DIR}/include/core/base
    ${CMAKE_SOURCE_DIR}/include/app
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/views
    ${CMAKE_SOURCE_DIR}/include/models
    ${CMAKE_SOURCE_DIR}/include/services
    ${CMAKE_SOURCE_DIR}/include/services/xts
    ${CMAKE_SOURCE_DIR}/include/api
    ${CMAKE_SOURCE_DIR}/include/repository
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/data
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsecm/include
)

# ========================================
# STABLE FRAMEWORK - Custom Qt Widgets
# ⚠️ These are tested and should rarely change
# ========================================
set(CORE_WIDGET_SOURCES
    src/core/widgets/CustomMainWindow.cpp
    src/core/widgets/CustomMDIArea.cpp
    src/core/widgets/CustomMDISubWindow.cpp
    src/core/widgets/CustomMDIChild.cpp
    src/core/widgets/CustomTitleBar.cpp
    src/core/widgets/CustomScripComboBox.cpp
    src/core/widgets/MDITaskBar.cpp
    src/core/widgets/InfoBar.cpp
    src/core/widgets/CustomMarketWatch.cpp
    src/core/widgets/CustomNetPosition.cpp
    src/core/widgets/CustomTradeBook.cpp
    src/core/widgets/CustomOrderBook.cpp
)

set(CORE_WIDGET_HEADERS
    include/core/widgets/CustomMainWindow.h
    include/core/widgets/CustomMDIArea.h
    include/core/widgets/CustomMDISubWindow.h
    include/core/widgets/CustomMDIChild.h
    include/core/widgets/CustomTitleBar.h
    include/core/widgets/CustomScripComboBox.h
    include/core/widgets/MDITaskBar.h
    include/core/widgets/InfoBar.h
    include/core/widgets/CustomMarketWatch.h
    include/core/widgets/CustomNetPosition.h
    include/core/widgets/CustomTradeBook.h
    include/core/widgets/CustomOrderBook.h
)

# ========================================
# APPLICATION LAYER
# ✅ Main application logic
# ========================================
set(APP_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/ScripBar.cpp
)

set(APP_HEADERS
    include/app/MainWindow.h
    include/app/ScripBar.h
)

# ========================================
# UI LAYER - Dialogs & Windows
# ✅ SplashScreen, LoginWindow, etc.
# ========================================
set(UI_SOURCES
    src/ui/SplashScreen.cpp
    src/ui/LoginWindow.cpp
)

set(UI_HEADERS
    include/ui/SplashScreen.h
    include/ui/LoginWindow.h
)

# ========================================
# VIEWS LAYER - Trading Windows
# ✅ Actively developed
# ========================================
set(VIEW_SOURCES
    src/views/BuyWindow.cpp
    src/views/SellWindow.cpp
    src/views/MarketWatchWindow.cpp
    src/views/SnapQuoteWindow.cpp
    src/views/PositionWindow.cpp
    src/views/TradeBookWindow.cpp
    src/views/OrderBookWindow.cpp
    src/views/ColumnProfileDialog.cpp
    src/views/PreferenceDialog.cpp
)

set(VIEW_HEADERS
    include/views/BuyWindow.h
    include/views/SellWindow.h
    include/views/MarketWatchWindow.h
    include/views/SnapQuoteWindow.h
    include/views/PositionWindow.h
    include/views/TradeBookWindow.h
    include/views/OrderBookWindow.h
    include/views/ColumnProfileDialog.h
    include/views/Preference.h
)

# ========================================
# MODELS LAYER - Data Models
# ✅ Actively developed
# ========================================
set(MODEL_SOURCES
    src/models/MarketWatchModel.cpp
    src/models/MarketWatchColumnProfile.cpp
    src/models/TokenAddressBook.cpp
)

set(MODEL_HEADERS
    include/models/MarketWatchModel.h
    include/models/MarketWatchColumnProfile.h
    include/models/TokenAddressBook.h
)

# ========================================
# SERVICES LAYER - Business Logic
# ✅ Actively developed (Future: XTS API integration)
# ========================================
set(SERVICE_SOURCES
    src/services/TokenSubscriptionManager.cpp
    src/services/LoginFlowService.cpp
    src/services/PriceCache.cpp
    src/services/FeedHandler.cpp
)

set(SERVICE_HEADERS
    include/services/TokenSubscriptionManager.h
    include/services/LoginFlowService.h
    include/services/PriceCache.h
    include/services/FeedHandler.h
)

# ========================================
# UTILS LAYER - Helper utilities
# ========================================
set(UTILS_SOURCES
    src/utils/PreferencesManager.cpp
)

set(UTILS_HEADERS
    include/utils/PreferencesManager.h
    include/models/WindowContext.h
)

# ========================================
# API LAYER - XTS API Integration
# ✅ XTS Market Data & Interactive API clients
# ✅ Native C++ network layer (698x faster than Qt)
# ========================================
set(API_SOURCES
    src/api/XTSMarketDataClient.cpp
    src/api/XTSInteractiveClient.cpp
    src/api/NativeWebSocketClient.cpp
    src/api/NativeHTTPClient.cpp
)

set(API_HEADERS
    include/api/XTSTypes.h
    include/api/XTSMarketDataClient.h
    include/api/XTSInteractiveClient.h
    include/api/NativeWebSocketClient.h
    include/api/NativeHTTPClient.h
)

# ========================================
# REPOSITORY LAYER - Data Access
# ✅ Master contract repositories with array-based search
# ========================================
set(REPOSITORY_SOURCES
    src/repository/Greeks.cpp
    src/repository/MasterFileParser.cpp
    src/repository/NSEFORepository.cpp
    src/repository/NSECMRepository.cpp
    src/repository/BSEFORepository.cpp
    src/repository/BSECMRepository.cpp
    src/repository/RepositoryManager.cpp
)

set(REPOSITORY_HEADERS
    include/repository/Greeks.h
    include/repository/ScripMaster.h
    include/repository/ContractData.h
    include/repository/MasterFileParser.h
    include/repository/NSEFORepository.h
    include/repository/NSECMRepository.h
    include/repository/BSEFORepository.h
    include/repository/BSECMRepository.h
    include/repository/RepositoryManager.h
)

# ========================================
# DATA LAYER - Market Data Aggregation
# ✅ Real-time UDP broadcast integration
# ========================================
set(DATA_SOURCES
    src/data/MarketDataAggregator.cpp
)

set(DATA_HEADERS
    include/data/MarketDataAggregator.h
)

# ========================================
# UTILITIES LAYER - Helper Functions
# ✅ Reusable utilities, preferences
# ========================================
set(UTILS_SOURCES
    src/utils/ClipboardHelpers.cpp
    src/utils/SelectionHelpers.cpp
    src/utils/ConfigLoader.cpp
    src/utils/PreferencesManager.cpp
)

set(UTILS_HEADERS
    include/utils/ClipboardHelpers.h
    include/utils/SelectionHelpers.h
    include/utils/ConfigLoader.h
    include/utils/PreferencesManager.h
    include/models/WindowContext.h
)

# ========================================
# VIEW HELPERS - View-specific utilities
# ✅ Domain-specific helper functions
# ========================================
set(VIEW_HELPER_SOURCES
    src/views/helpers/MarketWatchHelpers.cpp
    src/views/helpers/PositionHelpers.cpp
)

set(VIEW_HELPER_HEADERS
    include/views/helpers/MarketWatchHelpers.h
    include/views/helpers/PositionHelpers.h
)

# ========================================
# COMBINE ALL SOURCES
# ========================================
set(SOURCES
    ${CORE_WIDGET_SOURCES}
    ${APP_SOURCES}
    ${UI_SOURCES}
    ${VIEW_SOURCES}
    ${MODEL_SOURCES}
    ${SERVICE_SOURCES}
    ${API_SOURCES}
    ${REPOSITORY_SOURCES}
    ${DATA_SOURCES}
    ${UTILS_SOURCES}
    ${VIEW_HELPER_SOURCES}
)

set(HEADERS
    ${CORE_WIDGET_HEADERS}
    ${APP_HEADERS}
    ${UI_HEADERS}
    ${VIEW_HEADERS}
    ${MODEL_HEADERS}
    ${UTILS_HEADERS}
    ${VIEW_HELPER_HEADERS}
    ${SERVICE_HEADERS}
    ${API_HEADERS}
    ${REPOSITORY_HEADERS}
    ${DATA_HEADERS}
)

# UI files
set(FORMS
    resources/forms/InfoBar.ui
    resources/forms/SplashScreen.ui
    resources/forms/LoginWindow.ui
)

# Resource files
set(RESOURCES
    resources/resources.qrc
)

# ========================================
# BROADCAST RECEIVER LIBRARIES
# ✅ Native C++ UDP multicast receivers
# ========================================

# NSE FO Broadcast Library
set(NSEFO_BROADCAST_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/multicast_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/udp_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/nse_parsers.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/lzo_decompress.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/lzo1z_decompress_lib.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/config.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/utils/parse_compressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/utils/parse_uncompressed_message.cpp
    # Message parsers
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7200.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7202.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7208.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7211.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7220.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_17201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_17202.cpp
)

# Add executable
add_executable(TradingTerminal
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
    ${NSEFO_BROADCAST_SOURCES}
)

# Link libraries
target_link_libraries(TradingTerminal PRIVATE
    Qt5::Widgets
    Qt5::Network
    Qt5::WebSockets
    Qt5::UiTools
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    lzo2
)

# Include Boost headers (Beast is header-only)
target_include_directories(TradingTerminal PRIVATE ${Boost_INCLUDE_DIRS})

# macOS Bundle Settings
if(APPLE)
    set_target_properties(TradingTerminal PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "TradingTerminal"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.trading.terminal"
        MACOSX_BUNDLE_INFO_STRING "Professional Trading Terminal"
    )
endif()

# Add tests subdirectory
add_subdirectory(tests)

# XTS API Test Program (disabled due to GCC 9 compiler ICE bug)
# add_executable(test_xts_api test_xts_api.cpp)
# target_link_libraries(test_xts_api Qt5::Core Qt5::Network)
# set_target_properties(test_xts_api PROPERTIES AUTOMOC ON)

# Performance Benchmark: Qt vs Native C++
add_executable(benchmark_qt_vs_native tests/benchmark_qt_vs_native.cpp)
target_link_libraries(benchmark_qt_vs_native Qt5::Core)
set_target_properties(benchmark_qt_vs_native PROPERTIES AUTOMOC ON)
