# ========================================
# INDICATORS LAYER - Technical Analysis (TA-Lib Wrapper)
# ========================================

add_library(indicators STATIC
    TALibIndicators.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/indicators/TALibIndicators.h
)

target_link_libraries(indicators PUBLIC
    Qt5::Core
)

# Add TA-Lib if found
if(TALIB_FOUND)
    target_link_libraries(indicators PUBLIC ${TALIB_LIBRARY})
    target_include_directories(indicators PUBLIC ${TALIB_INCLUDE_DIR})
    target_compile_definitions(indicators PUBLIC HAVE_TALIB)
endif()
