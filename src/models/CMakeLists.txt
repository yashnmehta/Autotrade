# ========================================
# MODELS LAYER - Qt MVC Models, Profiles, Domain Types
# ========================================

add_library(models STATIC
    MarketWatchModel.cpp
    MarketWatchColumnProfile.cpp
    TokenAddressBook.cpp
    OrderModel.cpp
    TradeModel.cpp
    PositionModel.cpp
    PinnedRowProxyModel.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/models/qt/MarketWatchModel.h
    ${CMAKE_SOURCE_DIR}/include/models/qt/OrderModel.h
    ${CMAKE_SOURCE_DIR}/include/models/qt/TradeModel.h
    ${CMAKE_SOURCE_DIR}/include/models/qt/PositionModel.h
    ${CMAKE_SOURCE_DIR}/include/models/qt/PinnedRowProxyModel.h
    ${CMAKE_SOURCE_DIR}/include/models/profiles/MarketWatchColumnProfile.h
    ${CMAKE_SOURCE_DIR}/include/models/profiles/GenericProfileManager.h
    ${CMAKE_SOURCE_DIR}/include/models/profiles/GenericTableProfile.h
    ${CMAKE_SOURCE_DIR}/include/models/domain/TokenAddressBook.h
    ${CMAKE_SOURCE_DIR}/include/models/domain/WindowContext.h
    ${CMAKE_SOURCE_DIR}/include/models/interfaces/IMarketWatchViewCallback.h
)

target_link_libraries(models PUBLIC
    Qt5::Widgets
)
