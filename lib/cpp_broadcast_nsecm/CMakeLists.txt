cmake_minimum_required(VERSION 3.10)
project(cpp_broadcast_nsecm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/nsecm_price_store.cpp
    src/config.cpp
    src/logger.cpp
    src/lzo_decompress.cpp
    src/lzo1z_decompress_lib.cpp
    src/multicast_receiver.cpp
    src/udp_receiver.cpp
    src/nse_parsers.cpp
    src/parser/parse_broadcash_header.cpp
    src/parser/parse_message_5295.cpp
    src/parser/parse_message_6013.cpp
    src/parser/parse_message_6501.cpp
    src/parser/parse_message_6511.cpp
    src/parser/parse_message_6521.cpp
    src/parser/parse_message_6522.cpp
    src/parser/parse_message_6531.cpp
    src/parser/parse_message_6541.cpp
    src/parser/parse_message_6571.cpp
    src/parser/parse_message_7200.cpp
    src/parser/parse_message_7201.cpp
    src/parser/parse_message_7203.cpp
    src/parser/parse_message_7206.cpp
    src/parser/parse_message_7207.cpp
    src/parser/parse_message_7208.cpp
    src/parser/parse_message_7210.cpp
    src/parser/parse_message_9010.cpp
    src/parser/parse_message_9011.cpp
    src/parser/parse_message_18703.cpp
    src/parser/parse_message_18708.cpp
)

# Header files
set(HEADERS
    include/nsecm_callback.h
    include/nsecm_config.h
    include/nsecm_logger.h
    include/nsecm_multicast_receiver.h
    include/nsecm_udp_receiver.h
    include/nsecm_packet.h
    include/nsecm_price_store.h
    include/nse_parsers.h
    include/protocol.h
    include/broadcast_header.h
    include/constants.h
    include/lzo_decompress.h
    include/performance.h
)

# Create static library
add_library(cpp_broadcast_nsecm STATIC ${SOURCES} ${HEADERS})

# Link Qt6
target_link_libraries(cpp_broadcast_nsecm 
    PUBLIC Qt6::Core
)

# Export include directories
target_include_directories(cpp_broadcast_nsecm 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(cpp_broadcast_nsecm PRIVATE ws2_32)
endif()
