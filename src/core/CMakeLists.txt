# ========================================
# CORE WIDGETS - Custom Qt MDI Framework
# ⚠️ Stable framework — rarely changes
# ========================================

add_library(core_widgets STATIC
    # Widget sources
    widgets/CustomMainWindow.cpp
    widgets/CustomMDIArea.cpp
    widgets/CustomMDISubWindow.cpp
    widgets/CustomMDIChild.cpp
    widgets/CustomTitleBar.cpp
    widgets/CustomScripComboBox.cpp
    widgets/MDITaskBar.cpp
    widgets/InfoBar.cpp
    widgets/CustomMarketWatch.cpp
    widgets/CustomNetPosition.cpp
    widgets/CustomTradeBook.cpp
    widgets/CustomOrderBook.cpp
    GlobalShortcuts.cpp
    GlobalConnections.cpp
    WindowCacheManager.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomMainWindow.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomMDIArea.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomMDISubWindow.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomMDIChild.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomTitleBar.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomScripComboBox.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/MDITaskBar.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/InfoBar.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomMarketWatch.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomNetPosition.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomTradeBook.h
    ${CMAKE_SOURCE_DIR}/include/core/widgets/CustomOrderBook.h
    ${CMAKE_SOURCE_DIR}/include/core/WindowCacheManager.h
    ${CMAKE_SOURCE_DIR}/include/core/WindowConstants.h
    ${CMAKE_SOURCE_DIR}/include/core/ExchangeSegment.h
)

target_include_directories(core_widgets PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/platform
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/core/widgets
    ${CMAKE_SOURCE_DIR}/include/app
    ${CMAKE_SOURCE_DIR}/include/models
    ${CMAKE_SOURCE_DIR}/include/models/qt
    ${CMAKE_SOURCE_DIR}/include/models/profiles
    ${CMAKE_SOURCE_DIR}/include/models/domain
    ${CMAKE_SOURCE_DIR}/include/models/interfaces
    ${CMAKE_SOURCE_DIR}/include/services
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/data
    ${CMAKE_SOURCE_DIR}/include/views
    ${CMAKE_SOURCE_DIR}/include/repository
    ${CMAKE_SOURCE_DIR}/include/search
    ${CMAKE_SOURCE_DIR}/include/quant
    ${CMAKE_SOURCE_DIR}/include/api
    ${CMAKE_SOURCE_DIR}/include/api/transport
    ${CMAKE_SOURCE_DIR}/include/api/xts
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/strategy
    ${CMAKE_SOURCE_DIR}/include/strategy/model
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime
    ${CMAKE_SOURCE_DIR}/include/strategy/builder
    ${CMAKE_SOURCE_DIR}/include/strategy/manager
    ${CMAKE_SOURCE_DIR}/include/strategy/persistence
    ${CMAKE_SOURCE_DIR}/lib/common/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsecm/include
    ${LZO_INCLUDE_DIRS}
)

target_link_libraries(core_widgets PUBLIC
    Qt5::Widgets
    Qt5::Network
    Qt5::UiTools
)
