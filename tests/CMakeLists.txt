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

# Model Update Methods Benchmark
add_executable(benchmark_model_update_methods 
    benchmark_model_update_methods.cpp
)
target_include_directories(benchmark_model_update_methods PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(benchmark_model_update_methods
    Qt5::Core
    Qt5::Widgets
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

# ImGui vs Qt Benchmark
find_package(OpenGL REQUIRED)
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    # Try finding it via pkg-config if find_package fails
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/lib/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(benchmark_imgui_vs_qt 
    benchmark_imgui_vs_qt.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(benchmark_imgui_vs_qt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

if(glfw3_FOUND)
    target_link_libraries(benchmark_imgui_vs_qt
        ${OPENGL_LIBRARIES}
        glfw
        dl
    )
else()
    target_link_libraries(benchmark_imgui_vs_qt
        ${OPENGL_LIBRARIES}
        ${GLFW_LIBRARIES}
        dl
    )
    target_include_directories(benchmark_imgui_vs_qt PRIVATE ${GLFW_INCLUDE_DIRS})
endif()

set_target_properties(benchmark_imgui_vs_qt
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Add subdirectory for signal vs callback test
add_subdirectory(QtSignalVsCppCallback_test)

# Add subdirectory for broadcast message test
add_subdirectory(broadcast_message_test)

