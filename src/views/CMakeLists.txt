# ========================================
# VIEWS LAYER - MDI Trading Windows
# ========================================

add_library(views STATIC
    BaseOrderWindow.cpp
    BaseBookWindow.cpp
    BuyWindow.cpp
    SellWindow.cpp
    ATMWatchWindow.cpp
    ATMWatchSettingsDialog.cpp
    OptionChainWindow.cpp
    OptionCalculatorWindow.cpp
    MarketWatchWindow/MarketWatchWindow.cpp
    MarketWatchWindow/UI.cpp
    MarketWatchWindow/Data.cpp
    MarketWatchWindow/Actions.cpp
    SnapQuoteWindow/SnapQuoteWindow.cpp
    SnapQuoteWindow/UI.cpp
    SnapQuoteWindow/Actions.cpp
    SnapQuoteWindow/Data.cpp
    PositionWindow/PositionWindow.cpp
    TradeBookWindow/TradeBookWindow.cpp
    OrderBookWindow/OrderBookWindow.cpp
    GenericProfileDialog.cpp
    PreferenceDialog/PreferenceDialog.cpp
    PreferenceDialog/PreferencesGeneralTab.cpp
    PreferenceDialog/PreferencesOrderTab.cpp
    PreferenceDialog/PreferencesWorkSpaceTab.cpp
    PreferenceDialog/PreferencesPortfolioTab.cpp
    CustomizeDialog.cpp
    IndicesView.cpp
    AllIndicesWindow.cpp
    MarketMovementWindow.cpp

    # View helpers
    helpers/MarketWatchHelpers.cpp
    helpers/PositionHelpers.cpp
    helpers/GenericTableFilter.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/views/BaseOrderWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/BaseBookWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/BuyWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/SellWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/ATMWatchWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/ATMWatchSettingsDialog.h
    ${CMAKE_SOURCE_DIR}/include/views/OptionChainWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/OptionCalculatorWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/MarketWatchWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/SnapQuoteWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/PositionWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/TradeBookWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/OrderBookWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/GenericProfileDialog.h
    ${CMAKE_SOURCE_DIR}/include/views/PreferenceDialog.h
    ${CMAKE_SOURCE_DIR}/include/views/PreferencesGeneralTab.h
    ${CMAKE_SOURCE_DIR}/include/views/PreferencesOrderTab.h
    ${CMAKE_SOURCE_DIR}/include/views/PreferencesWorkSpaceTab.h
    ${CMAKE_SOURCE_DIR}/include/views/PreferencesPortfolioTab.h
    ${CMAKE_SOURCE_DIR}/include/views/CustomizeDialog.h
    ${CMAKE_SOURCE_DIR}/include/views/IndicesView.h
    ${CMAKE_SOURCE_DIR}/include/views/AllIndicesWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/MarketMovementWindow.h
    ${CMAKE_SOURCE_DIR}/include/views/helpers/MarketWatchHelpers.h
    ${CMAKE_SOURCE_DIR}/include/views/helpers/PositionHelpers.h
    ${CMAKE_SOURCE_DIR}/include/views/helpers/GenericTableFilter.h
)

target_link_libraries(views PUBLIC
    Qt5::Widgets
    Qt5::Network
    Qt5::UiTools
    Qt5::Concurrent
)
