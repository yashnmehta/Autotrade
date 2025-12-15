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

# Find Qt5 components
find_package(Qt5 COMPONENTS Widgets Network WebSockets UiTools REQUIRED)

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
)

set(VIEW_HEADERS
    include/views/BuyWindow.h
    include/views/SellWindow.h
    include/views/MarketWatchWindow.h
    include/views/SnapQuoteWindow.h
    include/views/PositionWindow.h
)

# ========================================
# MODELS LAYER - Data Models
# ✅ Actively developed
# ========================================
set(MODEL_SOURCES
    src/models/MarketWatchModel.cpp
    src/models/TokenAddressBook.cpp
)

set(MODEL_HEADERS
    include/models/MarketWatchModel.h
    include/models/TokenAddressBook.h
)

# ========================================
# SERVICES LAYER - Business Logic
# ✅ Actively developed (Future: XTS API integration)
# ========================================
set(SERVICE_SOURCES
    src/services/TokenSubscriptionManager.cpp
    src/services/LoginFlowService.cpp
)

set(SERVICE_HEADERS
    include/services/TokenSubscriptionManager.h
    include/services/LoginFlowService.h
)

# ========================================
# API LAYER - XTS API Integration
# ✅ XTS Market Data & Interactive API clients
# ========================================
set(API_SOURCES
    src/api/XTSMarketDataClient.cpp
    src/api/XTSInteractiveClient.cpp
)

set(API_HEADERS
    include/api/XTSTypes.h
    include/api/XTSMarketDataClient.h
    include/api/XTSInteractiveClient.h
)

# ========================================
# REPOSITORY LAYER - Data Access
# ✅ Actively developed
# ========================================
set(REPOSITORY_SOURCES
    src/repository/Greeks.cpp
)

set(REPOSITORY_HEADERS
    include/repository/Greeks.h
    include/repository/ScripMaster.h
)

# ========================================
# UTILITIES LAYER - Helper Functions
# ✅ Reusable utilities
# ========================================
set(UTILS_SOURCES
    src/utils/ClipboardHelpers.cpp
    src/utils/SelectionHelpers.cpp
    src/utils/ConfigLoader.cpp
)

set(UTILS_HEADERS
    include/utils/ClipboardHelpers.h
    include/utils/SelectionHelpers.h
    include/utils/ConfigLoader.h
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

# Add executable
add_executable(TradingTerminal
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
)

# Link libraries
target_link_libraries(TradingTerminal PRIVATE
    Qt5::Widgets
    Qt5::Network
    Qt5::WebSockets
    Qt5::UiTools
)

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
