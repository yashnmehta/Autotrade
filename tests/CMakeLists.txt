# XTS API Test Programs
cmake_minimum_required(VERSION 3.16)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Network)

# Enable automoc for Qt meta-object compilation
set(CMAKE_AUTOMOC ON)

# Market Data API Test
add_executable(test_marketdata_api test_marketdata_api.cpp)
target_link_libraries(test_marketdata_api
    Qt5::Core
    Qt5::Network
)

# Interactive API Test
add_executable(test_interactive_api test_interactive_api.cpp)
target_link_libraries(test_interactive_api
    Qt5::Core
    Qt5::Network
)

# Set output directory for test binaries
set_target_properties(test_marketdata_api test_interactive_api
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
