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

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/api
    ${CMAKE_SOURCE_DIR}/include/data
)

# Source files
set(SOURCES
    src/main.cpp
    src/ui/CustomMainWindow.cpp
    src/ui/MainWindow.cpp
    src/widgets/CustomTitleBar.cpp
    src/widgets/CustomMDIArea.cpp
    src/widgets/CustomMDISubWindow.cpp
    src/widgets/MDITaskBar.cpp
    src/ui/ScripBar.cpp
    src/ui/CustomScripComboBox.cpp
    src/ui/InfoBar.cpp
    src/data/Greeks.cpp
    # Market Watch components
    src/ui/MarketWatchModel.cpp
    src/ui/MarketWatchWindow.cpp
    src/ui/TokenAddressBook.cpp
    src/api/TokenSubscriptionManager.cpp
    # Order and Quote windows
    src/ui/BuyWindow.cpp
    src/ui/SellWindow.cpp
    src/ui/SnapQuoteWindow.cpp
    src/ui/PositionWindow.cpp
)

# Header files (for MOC)
set(HEADERS
    include/ui/CustomMainWindow.h
    include/ui/MainWindow.h
    include/ui/CustomTitleBar.h
    include/ui/CustomMDIArea.h
    include/ui/CustomMDISubWindow.h
    include/ui/MDITaskBar.h
    include/ui/ScripBar.h
    include/ui/CustomScripComboBox.h
    include/ui/InfoBar.h
    include/data/Greeks.h
    include/data/ScripMaster.h
    # Market Watch components
    include/ui/MarketWatchModel.h
    include/ui/MarketWatchWindow.h
    include/ui/TokenAddressBook.h
    include/api/TokenSubscriptionManager.h
    # Order and Quote windows
    include/ui/BuyWindow.h
    include/ui/SellWindow.h
    include/ui/SnapQuoteWindow.h
    include/ui/PositionWindow.h
)

# UI files
set(FORMS
    resources/forms/InfoBar.ui
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
