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

# Use OpenSSL from C:/Program Files/OpenSSL-Win64 (native Windows build)
if(FALSE) # Disabled manual config due to missing lib files
    message(STATUS "Detected native Windows OpenSSL installation")
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64" CACHE PATH "OpenSSL root directory" FORCE)
    set(OPENSSL_INCLUDE_DIR "C:/Program Files/OpenSSL-Win64/include" CACHE PATH "OpenSSL include directory" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/libcrypto.lib" CACHE FILEPATH "OpenSSL crypto library" FORCE)
    set(OPENSSL_SSL_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/libssl.lib" CACHE FILEPATH "OpenSSL SSL library" FORCE)
    
    # Verify OpenSSL files exist
    if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
        message(FATAL_ERROR "OpenSSL headers not found at ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
    endif()
    if(NOT EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
        message(FATAL_ERROR "OpenSSL crypto library not found at ${OPENSSL_CRYPTO_LIBRARY}")
    endif()
    if(NOT EXISTS "${OPENSSL_SSL_LIBRARY}")
        message(FATAL_ERROR "OpenSSL SSL library not found at ${OPENSSL_SSL_LIBRARY}")
    endif()
    
    message(STATUS "Using native Windows OpenSSL 1.1.1")
    message(STATUS "  Include dir: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "  Crypto library: ${OPENSSL_CRYPTO_LIBRARY}")
    message(STATUS "  SSL library: ${OPENSSL_SSL_LIBRARY}")
    
    # Set the found flag manually since we're bypassing find_package
    set(OPENSSL_FOUND TRUE CACHE BOOL "OpenSSL found" FORCE)
    set(OPENSSL_VERSION "1.1.1" CACHE STRING "OpenSSL version" FORCE)
    
    # Create imported targets manually (required for target_link_libraries)
    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        )
    endif()
    
    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        )
    endif()
# MSYS2 OpenSSL configuration removed to avoid toolchain mismatch.
# Relying on find_package(OpenSSL) or manual configuration via CMake variables.

endif()

# Only call find_package if OpenSSL wasn't manually configured
if(NOT OPENSSL_FOUND)
    find_package(OpenSSL REQUIRED)
endif()

find_package(Threads REQUIRED)

# ========================================
# Socket.IO C++ Client Library - DISABLED
# Official library has compilation issues with current compiler.
# Using NativeWebSocketClient with proper Socket.IO protocol handling instead.
# ========================================
# include(FetchContent)
# FetchContent_Declare(
#     sioclient
#     GIT_REPOSITORY https://github.com/socketio/socket.io-client-cpp.git
#     GIT_TAG        3.1.0
# )
# set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
# FetchContent_MakeAvailable(sioclient)

# ========================================
# LZO Library Configuration (Cross-platform)
# ========================================
option(USE_SYSTEM_LZO "Use system-installed LZO library instead of bundled one" ON)

if(USE_SYSTEM_LZO)
    # Try to find system LZO library
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LZO lzo2)
    endif()

    if(NOT LZO_FOUND)
        # Fallback to find_library
        find_library(LZO_LIBRARY
            NAMES lzo2 liblzo2
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib 
                  C:/vcpkg/installed/x64-windows/lib C:/vcpkg/installed/x86/lib
        )
        find_path(LZO_INCLUDE_DIR
            NAMES lzo/lzo1z.h
            PATHS /usr/include /usr/local/include /opt/homebrew/include
                  C:/vcpkg/installed/x64-windows/include C:/vcpkg/installed/x86/include
        )

        if(LZO_LIBRARY AND LZO_INCLUDE_DIR)
            set(LZO_FOUND TRUE)
            set(LZO_LIBRARIES ${LZO_LIBRARY})
            set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
        endif()
    endif()

    if(LZO_FOUND)
        message(STATUS "Using system LZO library")
        message(STATUS "LZO include dirs: ${LZO_INCLUDE_DIRS}")
        message(STATUS "LZO libraries: ${LZO_LIBRARIES}")
        set(USE_BUNDLED_LZO OFF)
    else()
        message(STATUS "System LZO library not found, using bundled version")
        set(USE_BUNDLED_LZO ON)
    endif()
else()
    message(STATUS "Using bundled LZO library (USE_SYSTEM_LZO=OFF)")
    set(USE_BUNDLED_LZO ON)
endif()

if(USE_BUNDLED_LZO)
    # Build bundled LZO library
    add_subdirectory(lib/lzo-2.10)
    set(LZO_LIBRARIES lzo_static_lib)
    set(LZO_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/lib/lzo-2.10/include)
endif()

# Export LZO variables for use in subdirectories (available via cache)
set(LZO_LIBRARIES ${LZO_LIBRARIES} CACHE INTERNAL "LZO library path")
set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIRS} CACHE INTERNAL "LZO include directories")
set(USE_BUNDLED_LZO ${USE_BUNDLED_LZO} CACHE INTERNAL "Whether using bundled LZO")

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
    ${CMAKE_SOURCE_DIR}/lib/common/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsecm/include
    ${LZO_INCLUDE_DIRS}
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
    src/core/GlobalShortcuts.cpp
    src/core/GlobalConnections.cpp
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
    src/app/MainWindow/MainWindow.cpp
    src/app/MainWindow/UI.cpp
    src/app/MainWindow/Windows.cpp
    src/app/MainWindow/Network.cpp
    src/app/MainWindow/WindowCycling.cpp
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
    src/ui/OptionChainWindow.cpp
)

set(UI_HEADERS
    include/ui/SplashScreen.h
    include/ui/LoginWindow.h
    include/ui/OptionChainWindow.h
)

# ========================================
# VIEWS LAYER - Trading Windows
# ✅ Actively developed
# ========================================
set(VIEW_SOURCES
    src/views/BaseOrderWindow.cpp
    src/views/BaseBookWindow.cpp
    src/views/BuyWindow.cpp
    src/views/SellWindow.cpp
    src/views/MarketWatchWindow/MarketWatchWindow.cpp
    src/views/MarketWatchWindow/UI.cpp
    src/views/MarketWatchWindow/Data.cpp
    src/views/MarketWatchWindow/Actions.cpp
    src/views/SnapQuoteWindow/SnapQuoteWindow.cpp
    src/views/SnapQuoteWindow/UI.cpp
    src/views/SnapQuoteWindow/Actions.cpp
    src/views/SnapQuoteWindow/Data.cpp
    src/views/PositionWindow/PositionWindow.cpp
    src/views/TradeBookWindow/TradeBookWindow.cpp
    src/views/OrderBookWindow/OrderBookWindow.cpp
    src/views/ColumnProfileDialog.cpp
    src/views/GenericProfileDialog.cpp
    src/views/PreferenceDialog/PreferenceDialog.cpp
    src/views/PreferenceDialog/PreferencesGeneralTab.cpp
    src/views/PreferenceDialog/PreferencesOrderTab.cpp
    src/views/CustomizeDialog.cpp
    src/views/IndicesView.cpp
)

set(VIEW_HEADERS
    include/views/BaseOrderWindow.h
    include/views/BaseBookWindow.h
    include/views/BuyWindow.h
    include/views/SellWindow.h
    include/views/MarketWatchWindow.h
    include/views/SnapQuoteWindow.h
    include/ui/OptionChainWindow.h
    include/views/PositionWindow.h
    include/views/TradeBookWindow.h
    include/views/OrderBookWindow.h
    include/views/ColumnProfileDialog.h
    include/views/GenericProfileDialog.h
    include/views/PreferenceDialog.h
    include/views/PreferencesGeneralTab.h
    include/views/PreferencesOrderTab.h
    include/views/CustomizeDialog.h
    include/views/IndicesView.h
)

# ========================================
# MODELS LAYER - Data Models
# ✅ Actively developed
# ========================================
set(MODEL_SOURCES
    src/models/MarketWatchModel.cpp
    src/models/MarketWatchColumnProfile.cpp
    src/models/TokenAddressBook.cpp
    src/models/OrderModel.cpp
    src/models/TradeModel.cpp
    src/models/PositionModel.cpp
    src/models/PinnedRowProxyModel.cpp
)

set(MODEL_HEADERS
    include/models/MarketWatchModel.h
    include/models/MarketWatchColumnProfile.h
    include/models/TokenAddressBook.h
    include/models/OrderModel.h
    include/models/TradeModel.h
    include/models/PositionModel.h
    include/models/PinnedRowProxyModel.h
)

# ========================================
# SERVICES LAYER - Business Logic
# ✅ Actively developed (Future: XTS API integration)
# ========================================
set(SERVICE_SOURCES
    src/services/TokenSubscriptionManager.cpp
    src/services/LoginFlowService.cpp
    src/services/TradingDataService.cpp
    src/services/PriceCache.cpp
    src/services/PriceCacheZeroCopy.cpp
    src/services/FeedHandler.cpp
    src/services/UdpBroadcastService.cpp
    src/services/MasterLoaderWorker.cpp
    src/services/MasterDataState.cpp
)

set(SERVICE_HEADERS
    include/services/TokenSubscriptionManager.h
    include/services/LoginFlowService.h
    include/services/TradingDataService.h
    include/services/PriceCache.h
    include/services/PriceCacheZeroCopy.h
    include/services/FeedHandler.h
    include/services/UdpBroadcastService.h
    include/services/MasterLoaderWorker.h
    include/services/MasterDataState.h
)

# ========================================
# UTILS LAYER - Helper utilities
# ========================================
# (Moved to line 362 - see below)

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
    # src/api/SocketIOInteractiveClient.cpp  # Disabled - using NativeWebSocketClient instead
)

set(API_HEADERS
    include/api/XTSTypes.h
    include/api/XTSMarketDataClient.h
    include/api/XTSInteractiveClient.h
    include/api/NativeWebSocketClient.h
    include/api/NativeHTTPClient.h
    # include/api/SocketIOInteractiveClient.h  # Disabled
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
    src/utils/MemoryProfiler.cpp
    src/utils/PreferencesManager.cpp
    src/utils/SoundManager.cpp
)

set(UTILS_HEADERS
    include/utils/ClipboardHelpers.h
    include/utils/SelectionHelpers.h
    include/utils/ConfigLoader.h
    include/utils/PreferencesManager.h
    include/utils/SoundManager.h
    include/models/WindowContext.h
)

# ========================================
# VIEW HELPERS - View-specific utilities
# ✅ Domain-specific helper functions
# ========================================
set(VIEW_HELPER_SOURCES
    src/views/helpers/MarketWatchHelpers.cpp
    src/views/helpers/PositionHelpers.cpp
    src/views/helpers/GenericTableFilter.cpp
)

set(VIEW_HELPER_HEADERS
    include/views/helpers/MarketWatchHelpers.h
    include/views/helpers/PositionHelpers.h
    include/views/helpers/GenericTableFilter.h
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
    resources/forms/customize.ui
    resources/forms/PreferencesWindowTab.ui
    resources/forms/PreferencesGeneralTab.ui
    resources/forms/PreferencesOrderTab.ui
    resources/forms/PreferencesDerivativeTab.ui
    resources/forms/PreferencesAlertMessageTab.ui
    resources/forms/PreferencesMarginPlusOrderTab.ui
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
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/config.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/utils/parse_compressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/utils/parse_uncompressed_message.cpp
    # Message parsers
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7200.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7202.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7203.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7207.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7208.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7211.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_7220.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_17201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/parser/parse_message_17202.cpp
)
# NSE CM Broadcast Library
set(NSECM_BROADCAST_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/multicast_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/udp_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/nse_parsers.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/config.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/utils/parse_compressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/utils/parse_uncompressed_message.cpp
    # Message parsers
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_5295.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6013.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7200.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7208.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_18703.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_18708.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7207.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7203.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6501.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7206.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7210.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6511.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6521.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6522.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6531.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6541.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6571.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_9010.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_9011.cpp
)


# Common LZO Library (shared by NSE CM and NSE FO)
set(COMMON_LZO_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/common/src/lzo_decompress.cpp
    ${CMAKE_SOURCE_DIR}/lib/common/src/lzo1z_decompress_lib.cpp
)

# BSE FO Broadcast Library
add_subdirectory(lib/cpp_broadcast_bsefo)
set(BSEFO_BROADCAST_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/src/bse_receiver.cpp
)


# Add executable
add_executable(TradingTerminal
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
    ${COMMON_LZO_SOURCES}
    ${NSEFO_BROADCAST_SOURCES}
    ${NSECM_BROADCAST_SOURCES}
    ${BSEFO_BROADCAST_SOURCES}
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
    ${LZO_LIBRARIES}
    cpp_broadcast_bsefo  # BSE FO broadcast library
    # sioclient  # Disabled
)

if(WIN32)
    target_link_libraries(TradingTerminal PRIVATE Ws2_32 Iphlpapi Psapi)
    if(MINGW)
        # Link winpthread from the current toolchain
        target_link_libraries(TradingTerminal PRIVATE winpthread)
        
        # Link libgcc and libstdc++ statically to avoid "Entry Point Not Found" (__cxa_thread_atexit)
        # and other DLL hell issues with different MinGW versions in PATH
        target_link_options(TradingTerminal PRIVATE -static-libgcc -static-libstdc++)
        message(STATUS "MinGW: Enabled static linking for libgcc and libstdc++")
    endif()
endif()

# Include Boost headers (Beast is header-only)
target_include_directories(TradingTerminal PRIVATE ${Boost_INCLUDE_DIRS})

# Include socket.io-client-cpp headers - DISABLED
# target_include_directories(TradingTerminal PRIVATE 
#     ${CMAKE_BINARY_DIR}/_deps/sioclient-src/src
# )

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
# add_subdirectory(tests)

# ========================================
# Post-Build: Copy DLLs for Windows MinGW
# ========================================
if(WIN32 AND MINGW)
    # Find windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt5_DIR}/../../../bin")
    
    if(WINDEPLOYQT_EXECUTABLE)
        # Deploy Qt DLLs automatically after build
        add_custom_command(TARGET TradingTerminal POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:TradingTerminal>"
            COMMENT "Running windeployqt to copy Qt DLLs..."
        )
        
        # Manually copy Qt Network and WebSockets DLLs (windeployqt sometimes misses these)
        get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
        get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
        add_custom_command(TARGET TradingTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_BIN_DIR}/Qt5Network.dll"
                "${QT_BIN_DIR}/Qt5WebSockets.dll"
                "$<TARGET_FILE_DIR:TradingTerminal>"
            COMMENT "Copying Qt Network and WebSockets DLLs..."
        )
    else()
        message(WARNING "windeployqt not found. Qt DLLs will need to be copied manually.")
    endif()
    
    # Copy vcpkg DLLs (OpenSSL, Boost if dynamic)
    if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "mingw-dynamic")
        file(GLOB VCPKG_DLLS 
            "${CMAKE_TOOLCHAIN_FILE}/../../../installed/${VCPKG_TARGET_TRIPLET}/bin/*.dll"
        )
        if(VCPKG_DLLS)
            add_custom_command(TARGET TradingTerminal POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VCPKG_DLLS} "$<TARGET_FILE_DIR:TradingTerminal>"
                COMMENT "Copying vcpkg DLLs..."
            )
        endif()
    endif()
    
    # Copy MinGW runtime DLLs from Chocolatey installation
    set(CHOCOLATEY_MINGW_BIN "C:/ProgramData/chocolatey/lib/mingw/tools/install/mingw64/bin")
    if(EXISTS "${CHOCOLATEY_MINGW_BIN}")
        add_custom_command(TARGET TradingTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CHOCOLATEY_MINGW_BIN}/libgcc_s_seh-1.dll"
                "${CHOCOLATEY_MINGW_BIN}/libstdc++-6.dll"
                "${CHOCOLATEY_MINGW_BIN}/libwinpthread-1.dll"
                "$<TARGET_FILE_DIR:TradingTerminal>"
            COMMENT "Copying MinGW runtime DLLs from Chocolatey..."
        )
    else()
        # Fallback: try to find MinGW DLLs relative to compiler
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB MINGW_RUNTIME_DLLS
            "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
            "${MINGW_BIN_DIR}/libstdc++-6.dll"
            "${MINGW_BIN_DIR}/libwinpthread-1.dll"
        )
        if(MINGW_RUNTIME_DLLS)
            add_custom_command(TARGET TradingTerminal POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_RUNTIME_DLLS} "$<TARGET_FILE_DIR:TradingTerminal>"
                COMMENT "Copying MinGW runtime DLLs..."
            )
        else()
            message(WARNING "MinGW runtime DLLs not found. Application may not run without them.")
        endif()
    endif()
endif()

# XTS API Test Program (disabled due to GCC 9 compiler ICE bug)
# add_executable(test_xts_api test_xts_api.cpp)
# target_link_libraries(test_xts_api Qt5::Core Qt5::Network)
# set_target_properties(test_xts_api PROPERTIES AUTOMOC ON)

# Performance Benchmark: Qt vs Native C++ (REMOVED - test file deleted)
# add_executable(benchmark_qt_vs_native tests/benchmark_qt_vs_native.cpp)
# target_link_libraries(benchmark_qt_vs_native Qt5::Core)
# set_target_properties(benchmark_qt_vs_native PROPERTIES AUTOMOC ON)
