cmake_minimum_required(VERSION 3.10)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core)

# Test executable
add_executable(test_broadcast_messages
    test_broadcast_messages.cpp
    ../../lib/cpp_broacast_nsefo/src/udp_receiver.cpp
    ../../lib/cpp_broacast_nsefo/src/multicast_receiver.cpp
    ../../lib/cpp_broacast_nsefo/src/logger.cpp
    ../../lib/cpp_broacast_nsefo/src/config.cpp
    ../../lib/cpp_broacast_nsefo/src/nse_parsers.cpp
    ../../lib/cpp_broacast_nsefo/src/lzo_decompress.cpp
    ../../lib/cpp_broacast_nsefo/src/lzo1z_decompress_lib.cpp
    ../../lib/cpp_broacast_nsefo/src/utils/parse_compressed_message.cpp
    ../../lib/cpp_broacast_nsefo/src/utils/parse_uncompressed_message.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_7200.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_7201.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_7202.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_7208.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_7211.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_7220.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_17201.cpp
    ../../lib/cpp_broacast_nsefo/src/parser/parse_message_17202.cpp
)

# Include directories
target_include_directories(test_broadcast_messages PRIVATE
    ../../lib/cpp_broacast_nsefo/include
    ../../include
    ${LZO_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(test_broadcast_messages
    Qt5::Core
    pthread
    ${LZO_LIBRARIES}
)

# C++ standard
set_target_properties(test_broadcast_messages PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)
