# ========================================
# LIB - Broadcast Receivers + Common Libraries
# ========================================

# Common LZO decompression (shared by NSE FO/CM)
add_library(common_lzo STATIC
    common/src/lzo_decompress.cpp
    common/src/lzo1z_decompress_lib.cpp
)

target_include_directories(common_lzo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${LZO_INCLUDE_DIRS}
)

target_link_libraries(common_lzo PUBLIC
    ${LZO_LIBRARIES}
)

# NSE FO Broadcast Receiver (compiled as object library into main exe)
# Note: nsefo/nsecm are NOT standalone add_subdirectory targets — their
# sources are compiled directly into the main executable because they use
# Qt5 (not Qt6 as their standalone CMakeLists suggest) and share the
# parent project's include tree.

# LZO 2.10 bundled library (only if system LZO not found)
# Must be built before common_lzo which depends on it
if(USE_BUNDLED_LZO)
    # AUTOMOC OFF: lzo is pure C, must not inherit global CMAKE_AUTOMOC ON
    set(CMAKE_AUTOMOC OFF)
    add_subdirectory(lzo-2.10)
    set(CMAKE_AUTOMOC ON)
    set(LZO_LIBRARIES lzo_static_lib)
    set(LZO_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/lib/lzo-2.10/include)
endif()

# BSE FO Broadcast Receiver (proper standalone static lib)
# AUTOMOC OFF: cpp_broadcast_bsefo is pure C++, must not inherit CMAKE_AUTOMOC ON
set(CMAKE_AUTOMOC OFF)
add_subdirectory(cpp_broadcast_bsefo)
set(CMAKE_AUTOMOC ON)
