# ========================================
# SERVICES LAYER - Business Logic
# ========================================

add_library(services STATIC
    TokenSubscriptionManager.cpp
    LoginFlowService.cpp
    TradingDataService.cpp
    FeedHandler.cpp
    UdpBroadcastService.cpp
    XTSFeedBridge.cpp
    MasterLoaderWorker.cpp
    MasterDataState.cpp
    ATMWatchManager.cpp
    GreeksCalculationService.cpp

    # Chart & Indicator Services
    HistoricalDataStore.cpp
    CandleAggregator.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/services/TokenSubscriptionManager.h
    ${CMAKE_SOURCE_DIR}/include/services/LoginFlowService.h
    ${CMAKE_SOURCE_DIR}/include/services/TradingDataService.h
    ${CMAKE_SOURCE_DIR}/include/services/FeedHandler.h
    ${CMAKE_SOURCE_DIR}/include/services/UdpBroadcastService.h
    ${CMAKE_SOURCE_DIR}/include/services/XTSFeedBridge.h
    ${CMAKE_SOURCE_DIR}/include/services/MasterLoaderWorker.h
    ${CMAKE_SOURCE_DIR}/include/services/MasterDataState.h
    ${CMAKE_SOURCE_DIR}/include/services/ATMWatchManager.h
    ${CMAKE_SOURCE_DIR}/include/services/GreeksCalculationService.h
    ${CMAKE_SOURCE_DIR}/include/services/HistoricalDataStore.h
    ${CMAKE_SOURCE_DIR}/include/services/CandleAggregator.h
    ${CMAKE_SOURCE_DIR}/include/data/CandleData.h
)

target_link_libraries(services PUBLIC
    Qt5::Widgets
    Qt5::Network
    Qt5::Concurrent
    Qt5::Sql
)
