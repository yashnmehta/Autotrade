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

# Standalone Interactive Data Test (using NativeHTTPClient)
add_executable(test_ia_data_native 
    test_ia_data_native.cpp
    ../src/api/NativeHTTPClient.cpp
)
target_include_directories(test_ia_data_native PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_ia_data_native
    Qt5::Core
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
# Add Winsock library on Windows for Boost.Asio
if(WIN32)
    target_link_libraries(test_ia_data_native ws2_32 wsock32)
    if(MINGW)
        # MinGW needs explicit linking for pthread from msys64
        if(EXISTS "C:/msys64/mingw64/lib/libwinpthread.dll.a")
            target_link_libraries(test_ia_data_native "C:/msys64/mingw64/lib/libwinpthread.dll.a")
        else()
            target_link_libraries(test_ia_data_native winpthread)
        endif()
    endif()
endif()
# BSE Parser Test
add_executable(test_bse_parser test_bse_parser.cpp)
target_include_directories(test_bse_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
)
target_link_libraries(test_bse_parser
    cpp_broadcast_bsefo
)
if(WIN32)
    target_link_libraries(test_bse_parser ws2_32)
endif()

# Set output directory for test binaries
set_target_properties(test_marketdata_api test_interactive_api test_ia_data_native test_bse_parser
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Collect sources for dependencies
file(GLOB_RECURSE NSEFO_SOURCES "${CMAKE_SOURCE_DIR}/lib/cpp_broacast_nsefo/src/*.cpp")
list(FILTER NSEFO_SOURCES EXCLUDE REGEX "main.cpp")
list(FILTER NSEFO_SOURCES EXCLUDE REGEX "lzo")

file(GLOB_RECURSE NSECM_SOURCES "${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/*.cpp")
list(FILTER NSECM_SOURCES EXCLUDE REGEX "main.cpp")
list(FILTER NSECM_SOURCES EXCLUDE REGEX "lzo")

# Common LZO
set(COMMON_LZO_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/common/src/lzo_decompress.cpp
    ${CMAKE_SOURCE_DIR}/lib/common/src/lzo1z_decompress_lib.cpp
)

# Repository Benchmark
add_executable(benchmark_repository_filters 
    benchmark_repository_filters.cpp
    ../src/repository/NSEFORepository.cpp
    ../src/repository/NSECMRepository.cpp
    ../src/repository/BSEFORepository.cpp
    ../src/repository/BSECMRepository.cpp
    ../src/repository/RepositoryManager.cpp
    ../src/repository/NSEFORepositoryPreSorted.cpp
    ../src/repository/MasterFileParser.cpp
    ../src/repository/Greeks.cpp
    ../src/utils/ConfigLoader.cpp
    ../src/utils/PreferencesManager.cpp
    ../src/utils/MemoryProfiler.cpp
    ../src/services/MasterDataState.cpp
    ../src/services/MasterLoaderWorker.cpp
    ${NSEFO_SOURCES}
    ${NSECM_SOURCES}
    ${COMMON_LZO_SOURCES}
    ../include/services/MasterLoaderWorker.h
    ../include/utils/PreferencesManager.h
    ../include/services/MasterDataState.h
    ../include/utils/ConfigLoader.h
)

target_include_directories(benchmark_repository_filters PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
    ${CMAKE_SOURCE_DIR}/include/repository
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/services
    ${CMAKE_SOURCE_DIR}/include/core
    ${Boost_INCLUDE_DIRS}
    ${LZO_INCLUDE_DIRS}
)

target_link_libraries(benchmark_repository_filters
    Qt5::Core
    Qt5::Network
    Qt5::Concurrent
    cpp_broadcast_bsefo
    ${LZO_LIBRARIES}
)

# Set output directory
set_target_properties(benchmark_repository_filters PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Simple load test (minimal dependencies)
add_executable(simple_load_test
    simple_load_test.cpp
    ../src/repository/RepositoryManager.cpp
    ../src/repository/NSEFORepositoryPreSorted.cpp
    ../src/repository/NSEFORepository.cpp
    ../src/repository/NSECMRepository.cpp
    ../src/repository/BSEFORepository.cpp
    ../src/repository/BSECMRepository.cpp
    ../src/repository/Greeks.cpp
    ../src/utils/ConfigLoader.cpp
    ../src/utils/PreferencesManager.cpp
    ../src/utils/MemoryProfiler.cpp
    ../src/services/MasterDataState.cpp
    ../src/services/MasterLoaderWorker.cpp
    ../src/repository/MasterFileParser.cpp
    ${NSEFO_SOURCES}
    ${NSECM_SOURCES}
    ${COMMON_LZO_SOURCES}
    ../include/services/MasterLoaderWorker.h
    ../include/utils/PreferencesManager.h
    ../include/services/MasterDataState.h
    ../include/utils/ConfigLoader.h
)

target_include_directories(simple_load_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsecm/include
    ${LZO_INCLUDE_DIRS}
)

target_link_libraries(simple_load_test
    Qt5::Core
    Qt5::Network
    Qt5::Concurrent
    cpp_broadcast_bsefo
    Threads::Threads
    ${LZO_LIBRARIES}
)

set_target_properties(simple_load_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Simple Repository Benchmark (minimal dependencies, no distributed stores)
add_executable(benchmark_repository_simple
    benchmark_repository_simple.cpp
    ../src/repository/NSEFORepository.cpp
    ../src/repository/MasterFileParser.cpp
    ../src/repository/Greeks.cpp
)

target_include_directories(benchmark_repository_simple PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/repository
)

target_link_libraries(benchmark_repository_simple
    Qt5::Core
)

set_target_properties(benchmark_repository_simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Comparison Benchmark: Current vs PreSorted (test/benchmark only)
add_executable(benchmark_comparison
    benchmark_comparison.cpp
    ../src/repository/NSEFORepository.cpp
    ../src/repository/NSEFORepositoryPreSorted.cpp
    ../src/repository/MasterFileParser.cpp
    ../src/repository/Greeks.cpp
)

target_include_directories(benchmark_comparison PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/repository
)

target_link_libraries(benchmark_comparison
    Qt5::Core
)

set_target_properties(benchmark_comparison PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# IV and Greeks Performance Benchmark
add_executable(benchmark_greeks
    benchmark_greeks.cpp
    ../src/repository/Greeks.cpp
    ../src/repository/IVCalculator.cpp
)

target_include_directories(benchmark_greeks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/repository
)

target_link_libraries(benchmark_greeks
    Qt5::Core
)

set_target_properties(benchmark_greeks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
