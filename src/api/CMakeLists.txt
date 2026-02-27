# ========================================
# API LAYER - Network Transport + XTS Broker Protocol
# ========================================

add_library(api_layer STATIC
    # Transport (generic network layer)
    transport/NativeHTTPClient.cpp
    transport/NativeWebSocketClient.cpp

    # XTS broker-specific protocol
    xts/XTSMarketDataClient.cpp
    xts/XTSInteractiveClient.cpp
    # xts/SocketIOInteractiveClient.cpp  # Disabled - using NativeWebSocketClient

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/api/transport/NativeHTTPClient.h
    ${CMAKE_SOURCE_DIR}/include/api/transport/NativeWebSocketClient.h
    ${CMAKE_SOURCE_DIR}/include/api/xts/XTSTypes.h
    ${CMAKE_SOURCE_DIR}/include/api/xts/XTSMarketDataClient.h
    ${CMAKE_SOURCE_DIR}/include/api/xts/XTSInteractiveClient.h
    # ${CMAKE_SOURCE_DIR}/include/api/xts/SocketIOInteractiveClient.h  # Disabled
    ${CMAKE_SOURCE_DIR}/include/api/IMarketDataProvider.h
    ${CMAKE_SOURCE_DIR}/include/api/UDPBroadcastProvider.h
    ${CMAKE_SOURCE_DIR}/include/api/NSEProtocol.h
)

target_link_libraries(api_layer PUBLIC
    Qt5::Widgets
    Qt5::Network
    Qt5::WebSockets
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_include_directories(api_layer PUBLIC
    ${Boost_INCLUDE_DIRS}
)
