cmake_minimum_required(VERSION 3.16)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core)

# Enable automoc for Qt meta-object compilation
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Signal vs Callback Communication Test
add_executable(test_signal_vs_callback_communication 
    test_signal_vs_callback_communication.cpp
)

target_link_libraries(test_signal_vs_callback_communication
    Qt5::Core
    Threads::Threads
)

# Set output directory
set_target_properties(test_signal_vs_callback_communication PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/QtSignalVsCppCallback_test
)
