cmake_minimum_required(VERSION 3.10)
project(cpp_broadcast_bsefo)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

# Define source files
set(SOURCES
    src/bse_receiver.cpp
    src/bse_parser.cpp
    src/bse_price_store.cpp
)

# Define headers
set(HEADERS
    include/bse_receiver.h
    include/bse_parser.h
    include/bse_protocol.h
    include/bse_utils.h
    include/bse_price_store.h
)

# Find Qt5 Core (required for QString in PriceCacheZeroCopy.h)
find_package(Qt5 COMPONENTS Core REQUIRED)

# Create library
add_library(cpp_broadcast_bsefo STATIC ${SOURCES} ${HEADERS})

# Link dependencies
target_link_libraries(cpp_broadcast_bsefo PRIVATE Qt5::Core)

# Add generic include directories if needed (e.g. for common UDP utils if used)
# target_include_directories(cpp_broadcast_bsefo PUBLIC include)
