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

# Price Cache Performance Benchmark
add_executable(benchmark_price_cache 
    benchmark_price_cache.cpp
    ../src/services/PriceCache.cpp
)
target_include_directories(benchmark_price_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/services
    ${CMAKE_SOURCE_DIR}/include/api
)
target_link_libraries(benchmark_price_cache
    Qt5::Core
    Threads::Threads
)

# Master Loading Test
add_executable(test_master_loading 
    test_master_loading.cpp
    ../src/repository/RepositoryManager.cpp
    ../src/repository/NSEFORepository.cpp
    ../src/repository/NSECMRepository.cpp
    ../src/repository/BSEFORepository.cpp
    ../src/repository/BSECMRepository.cpp
    ../src/repository/MasterFileParser.cpp
)
target_include_directories(test_master_loading PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/repository
    ${CMAKE_SOURCE_DIR}/include/data
)
target_link_libraries(test_master_loading
    Qt5::Core
    Threads::Threads
)

# Set output directory for test binaries
set_target_properties(test_marketdata_api test_interactive_api benchmark_price_cache test_master_loading
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
