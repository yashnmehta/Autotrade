# ========================================
# UI LAYER - App-Level Dialogs (Login, Splash, Search)
# ========================================

set(UI_DIALOG_SOURCES
    SplashScreen.cpp
    LoginWindow.cpp
    GlobalSearchWidget.cpp
    ConnectionBarWidget.cpp
    BroadcastSettingsDialog.cpp
)

set(UI_DIALOG_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ui/SplashScreen.h
    ${CMAKE_SOURCE_DIR}/include/ui/LoginWindow.h
    ${CMAKE_SOURCE_DIR}/include/ui/GlobalSearchWidget.h
    ${CMAKE_SOURCE_DIR}/include/ui/ConnectionBarWidget.h
    ${CMAKE_SOURCE_DIR}/include/ui/BroadcastSettingsDialog.h
)

# Add TradingView chart widget if enabled
if(ENABLE_TRADINGVIEW AND Qt5WebEngineWidgets_FOUND AND Qt5WebChannel_FOUND)
    list(APPEND UI_DIALOG_SOURCES TradingViewChartWidget.cpp)
    list(APPEND UI_DIALOG_HEADERS ${CMAKE_SOURCE_DIR}/include/ui/TradingViewChartWidget.h)
endif()

# Add Indicator Chart Widget if QtCharts is available
if(ENABLE_QTCHARTS AND Qt5Charts_FOUND)
    list(APPEND UI_DIALOG_SOURCES IndicatorChartWidget.cpp)
    list(APPEND UI_DIALOG_HEADERS ${CMAKE_SOURCE_DIR}/include/ui/IndicatorChartWidget.h)
endif()

add_library(ui_dialogs STATIC
    ${UI_DIALOG_SOURCES}
    ${UI_DIALOG_HEADERS}
)

target_link_libraries(ui_dialogs PUBLIC
    Qt5::Widgets
    Qt5::Network
)

# Add TradingView dependencies if enabled
if(ENABLE_TRADINGVIEW AND Qt5WebEngineWidgets_FOUND AND Qt5WebChannel_FOUND)
    target_link_libraries(ui_dialogs PUBLIC
        Qt5::WebEngineWidgets
        Qt5::WebChannel
    )
endif()

# Add QtCharts if enabled
if(ENABLE_QTCHARTS AND Qt5Charts_FOUND)
    target_link_libraries(ui_dialogs PUBLIC Qt5::Charts)
endif()
