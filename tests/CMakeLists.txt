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
set_target_properties(test_ia_data_native PROPERTIES AUTOMOC ON)

# Set output directory for test binaries
set_target_properties(test_marketdata_api test_interactive_api test_ia_data_native
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

message(STATUS "Test executables configured: test_marketdata_api, test_interactive_api, test_ia_data_native")
