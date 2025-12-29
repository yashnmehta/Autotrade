cmake_minimum_required(VERSION 3.10)
project(cpp_broadcast_bsefo)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

# Define source files
set(SOURCES
    src/bse_receiver.cpp
)

# Define headers
set(HEADERS
    include/bse_receiver.h
    include/bse_protocol.h
    include/bse_utils.h
)

# Create library
add_library(cpp_broadcast_bsefo STATIC ${SOURCES} ${HEADERS})

# Add generic include directories if needed (e.g. for common UDP utils if used)
# target_include_directories(cpp_broadcast_bsefo PUBLIC include)
