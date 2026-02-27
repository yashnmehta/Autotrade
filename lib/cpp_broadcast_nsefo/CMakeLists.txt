cmake_minimum_required(VERSION 3.10)
project(cpp_broacast_nsefo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/nsefo_price_store.cpp
    src/config.cpp
    src/logger.cpp
    src/lzo_decompress.cpp
    src/lzo1z_decompress_lib.cpp
    src/multicast_receiver.cpp
    src/udp_receiver.cpp
    src/nse_parsers.cpp
    src/parser/parse_broadcash_header.cpp
    src/parser/parse_message_7200.cpp
    src/parser/parse_message_7201.cpp
    src/parser/parse_message_7202.cpp
    src/parser/parse_message_7203.cpp
    src/parser/parse_message_7207.cpp
    src/parser/parse_message_7208.cpp
    src/parser/parse_message_7211.cpp
    src/parser/parse_message_7220.cpp
    src/parser/parse_message_17201.cpp
    src/parser/parse_message_17202.cpp
)

# Header files
set(HEADERS
    include/nsefo_callback.h
    include/nsefo_config.h
    include/nsefo_logger.h
    include/nsefo_multicast_receiver.h
    include/nsefo_udp_receiver.h
    include/nsefo_packet.h
    include/nsefo_price_store.h
    include/nse_parsers.h
    include/protocol.h
    include/broadcast_header.h
    include/constants.h
    include/lzo_decompress.h
    include/performance.h
)

# Create static library
add_library(cpp_broacast_nsefo STATIC ${SOURCES} ${HEADERS})

# Link Qt6
target_link_libraries(cpp_broacast_nsefo 
    PUBLIC Qt6::Core
)

# Export include directories
target_include_directories(cpp_broacast_nsefo 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(cpp_broacast_nsefo PRIVATE ws2_32)
endif()
