# ========================================
# DATA LAYER - Zero-Copy Price Stores
# ========================================

add_library(data_layer STATIC
    PriceStoreGateway.cpp
    SymbolCacheManager.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/data/PriceStoreGateway.h
    ${CMAKE_SOURCE_DIR}/include/data/UnifiedPriceState.h
    ${CMAKE_SOURCE_DIR}/include/data/SymbolCacheManager.h
)

target_link_libraries(data_layer PUBLIC
    Qt5::Widgets
)
