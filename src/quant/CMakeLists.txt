# ========================================
# QUANT LAYER - Mathematical Calculations
# ========================================

add_library(quant STATIC
    Greeks.cpp
    IVCalculator.cpp
    TimeToExpiry.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/quant/Greeks.h
    ${CMAKE_SOURCE_DIR}/include/quant/IVCalculator.h
    ${CMAKE_SOURCE_DIR}/include/quant/ATMCalculator.h
    ${CMAKE_SOURCE_DIR}/include/quant/TimeToExpiry.h
)

target_link_libraries(quant PUBLIC
    Qt5::Core
)
