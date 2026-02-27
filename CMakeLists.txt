cmake_minimum_required(VERSION 3.14)

# Fix MSVC parallel build PDB conflicts - use _INIT to ensure it persists
if(WIN32 AND NOT MINGW)
    set(CMAKE_CXX_FLAGS_INIT "/FS /MP")
    set(CMAKE_C_FLAGS_INIT "/FS /MP")
endif()

project(TradingTerminal LANGUAGES CXX)

# Disable unity builds - causes conflicts with duplicate symbols across broadcast libraries
set(CMAKE_UNITY_BUILD OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DNOMINMAX)
endif()

# MSVC optimizations for faster builds
if(MSVC)
    add_compile_options(/FS /MP)  # Multi-processor compilation
    add_compile_options(/W1)      # Reduce warning level for faster compilation

    # Faster linking options
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL")

    message(STATUS "MSVC: Enabled fast build optimizations (/FS /MP /W1 /INCREMENTAL)")
endif()

# Set AUTOUIC search paths for .ui files
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/resources/forms
    ${CMAKE_SOURCE_DIR}/resources/forms/strategy
)

# ========================================
# FIND PACKAGES
# ========================================

# Find Qt5 components
find_package(Qt5 COMPONENTS Widgets Network WebSockets UiTools Concurrent Sql REQUIRED)

# Find Boost for native WebSocket (Beast + Asio — header-only)
find_package(Boost 1.70 REQUIRED)

# Optional Qt Charts support
option(ENABLE_QTCHARTS "Enable Qt Charts for native charting" ON)
if(ENABLE_QTCHARTS)
    find_package(Qt5Charts QUIET)
    if(Qt5Charts_FOUND)
        message(STATUS "Qt Charts found. Enabling native QChart-based charting!")
        add_definitions(-DHAVE_QTCHARTS)
    else()
        message(WARNING "Qt Charts not found. Install with: Qt Maintenance Tool > Add Components > Qt Charts")
        message(STATUS "Continuing without native charts...")
    endif()
endif()

# Optional QtWebEngine support (Chromium for TradingView)
option(ENABLE_TRADINGVIEW "Enable TradingView Lightweight Charts integration" ON)
if(ENABLE_TRADINGVIEW)
    find_package(Qt5WebEngineWidgets QUIET)
    find_package(Qt5WebChannel QUIET)
    if(Qt5WebEngineWidgets_FOUND AND Qt5WebChannel_FOUND)
        message(STATUS "QtWebEngine found. Enabling TradingView chart integration!")
        add_definitions(-DHAVE_QTWEBENGINE)
        add_definitions(-DHAVE_TRADINGVIEW)
    else()
        message(STATUS "QtWebEngine not found. TradingView charts will be disabled.")
    endif()
endif()

# Include TA-Lib configuration
include(cmake/TALibConfig.cmake)

# ========================================
# OPENSSL
# ========================================
if(WIN32)
    message(STATUS "Detected native Windows OpenSSL installation")
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64" CACHE PATH "OpenSSL root directory" FORCE)
    set(OPENSSL_INCLUDE_DIR "C:/Program Files/OpenSSL-Win64/include" CACHE PATH "OpenSSL include directory" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/VC/x64/MD/libcrypto.lib" CACHE FILEPATH "OpenSSL crypto library" FORCE)
    set(OPENSSL_SSL_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/VC/x64/MD/libssl.lib" CACHE FILEPATH "OpenSSL SSL library" FORCE)

    if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
        message(FATAL_ERROR "OpenSSL headers not found at ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
    endif()
    if(NOT EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
        message(FATAL_ERROR "OpenSSL crypto library not found at ${OPENSSL_CRYPTO_LIBRARY}")
    endif()
    if(NOT EXISTS "${OPENSSL_SSL_LIBRARY}")
        message(FATAL_ERROR "OpenSSL SSL library not found at ${OPENSSL_SSL_LIBRARY}")
    endif()

    message(STATUS "Using native Windows OpenSSL 1.1.1")
    set(OPENSSL_FOUND TRUE CACHE BOOL "OpenSSL found" FORCE)
    set(OPENSSL_VERSION "1.1.1" CACHE STRING "OpenSSL version" FORCE)

    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        )
    endif()
endif()

# On non-Windows (macOS, Linux) use find_package
if(NOT OPENSSL_FOUND)
    find_package(OpenSSL REQUIRED)
endif()

find_package(Threads REQUIRED)

# ========================================
# TA-LIB (Optional)
# ========================================
option(ENABLE_TALIB "Enable TA-Lib for technical indicators" OFF)
if(ENABLE_TALIB)
    find_path(TALIB_INCLUDE_DIR ta_libc.h
        PATHS
            "C:/ta-lib/c/include"
            "C:/Program Files/TA-Lib/c/include"
            "C:/Program Files/TA-Lib/include"
            "C:/ta-lib/include"
            "${CMAKE_SOURCE_DIR}/lib/ta-lib/include"
            "/usr/local/include"
            "/usr/include"
    )

    find_library(TALIB_LIBRARY
        NAMES ta_libc_cdr ta_libc_cmd ta_lib ta_libc
        PATHS
            "C:/ta-lib/c/lib"
            "C:/Program Files/TA-Lib/c/lib"
            "C:/Program Files/TA-Lib/lib"
            "C:/ta-lib/lib"
            "${CMAKE_SOURCE_DIR}/lib/ta-lib/lib"
            "/usr/local/lib"
            "/usr/lib"
    )

    if(TALIB_INCLUDE_DIR AND TALIB_LIBRARY)
        message(STATUS "TA-Lib found!")
        message(STATUS "  Include: ${TALIB_INCLUDE_DIR}")
        message(STATUS "  Library: ${TALIB_LIBRARY}")
        add_definitions(-DHAVE_TALIB)
        set(TALIB_FOUND TRUE)
    else()
        message(WARNING "TA-Lib not found. Technical indicators will be limited.")
        set(TALIB_FOUND FALSE)
    endif()
endif()

# ========================================
# LZO LIBRARY (Cross-platform)
# ========================================
option(USE_SYSTEM_LZO "Use system-installed LZO library instead of bundled one" ON)

if(USE_SYSTEM_LZO)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LZO lzo2)
    endif()

    if(NOT LZO_FOUND)
        find_library(LZO_LIBRARY
            NAMES lzo2 liblzo2
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib
                  C:/vcpkg/installed/x64-windows/lib C:/vcpkg/installed/x86/lib
        )
        find_path(LZO_INCLUDE_DIR
            NAMES lzo/lzo1z.h
            PATHS /usr/include /usr/local/include /opt/homebrew/include
                  C:/vcpkg/installed/x64-windows/include C:/vcpkg/installed/x86/include
        )

        if(LZO_LIBRARY AND LZO_INCLUDE_DIR)
            set(LZO_FOUND TRUE)
            set(LZO_LIBRARIES ${LZO_LIBRARY})
            set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
        endif()
    endif()

    if(LZO_FOUND)
        message(STATUS "Using system LZO library")
        message(STATUS "LZO include dirs: ${LZO_INCLUDE_DIRS}")
        message(STATUS "LZO libraries: ${LZO_LIBRARIES}")
        set(USE_BUNDLED_LZO OFF)
    else()
        message(STATUS "System LZO library not found, using bundled version")
        set(USE_BUNDLED_LZO ON)
    endif()
else()
    message(STATUS "Using bundled LZO library (USE_SYSTEM_LZO=OFF)")
    set(USE_BUNDLED_LZO ON)
endif()

# Export LZO variables for subdirectories
set(LZO_LIBRARIES ${LZO_LIBRARIES} CACHE INTERNAL "LZO library path")
set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIRS} CACHE INTERNAL "LZO include directories")
set(USE_BUNDLED_LZO ${USE_BUNDLED_LZO} CACHE INTERNAL "Whether using bundled LZO")

# ========================================
# GLOBAL INCLUDE DIRECTORIES
# All modules share these via core_widgets PUBLIC includes,
# but we also set them globally for AUTOMOC/AUTOUIC to work
# correctly on .ui files and moc'd headers.
# ========================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/platform
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/core/widgets
    ${CMAKE_SOURCE_DIR}/include/app
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/views
    ${CMAKE_SOURCE_DIR}/include/models
    ${CMAKE_SOURCE_DIR}/include/models/qt
    ${CMAKE_SOURCE_DIR}/include/models/profiles
    ${CMAKE_SOURCE_DIR}/include/models/domain
    ${CMAKE_SOURCE_DIR}/include/models/interfaces
    ${CMAKE_SOURCE_DIR}/include/services
    ${CMAKE_SOURCE_DIR}/include/api
    ${CMAKE_SOURCE_DIR}/include/api/transport
    ${CMAKE_SOURCE_DIR}/include/api/xts
    ${CMAKE_SOURCE_DIR}/include/repository
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/data
    ${CMAKE_SOURCE_DIR}/include/strategy
    ${CMAKE_SOURCE_DIR}/include/strategy/model
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime
    ${CMAKE_SOURCE_DIR}/include/strategy/builder
    ${CMAKE_SOURCE_DIR}/include/strategy/manager
    ${CMAKE_SOURCE_DIR}/include/strategy/persistence
    ${CMAKE_SOURCE_DIR}/include/search
    ${CMAKE_SOURCE_DIR}/include/quant
    ${CMAKE_SOURCE_DIR}/lib/common/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/include
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsecm/include
    ${LZO_INCLUDE_DIRS}
)

# ========================================
# ADD SUBDIRECTORIES
# ========================================

# Third-party / internal libraries (LZO bundled, broadcast receivers)
add_subdirectory(lib)

# Main application (builds all module static libs + executable)
add_subdirectory(src)

# ========================================
# UNIT TESTS
# ========================================
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ========================================
# POST-BUILD: Windows DLL Deployment
# ========================================
if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    set(DLL_SUFFIX "")
    if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(DLL_SUFFIX "d")
    endif()

    set(QT_DEPS Core Gui Widgets Network WebSockets Sql Concurrent UiTools)

    foreach(DEP ${QT_DEPS})
        set(DLL_NAME "Qt5${DEP}${DLL_SUFFIX}.dll")
        if(EXISTS "${QT_BIN_DIR}/${DLL_NAME}")
            add_custom_command(TARGET TradingTerminal POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_BIN_DIR}/${DLL_NAME}"
                    "$<TARGET_FILE_DIR:TradingTerminal>"
                COMMENT "Deploying ${DLL_NAME}..."
            )
        endif()
    endforeach()

    # windeployqt for MSVC builds
    if(MSVC)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET TradingTerminal POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env "PATH=${QT_BIN_DIR};$ENV{SystemRoot}\\System32;$ENV{SystemRoot}"
                    "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-compiler-runtime
                    --dir "$<TARGET_FILE_DIR:TradingTerminal>"
                    "$<TARGET_FILE:TradingTerminal>"
                COMMENT "Running windeployqt to deploy Qt dependencies..."
            )
            message(STATUS "windeployqt found: ${WINDEPLOYQT_EXECUTABLE}")
        else()
            message(WARNING "windeployqt not found. You may need to manually copy Qt DLLs.")
        endif()

        # Copy OpenSSL DLLs
        set(OPENSSL_BIN_DIR "C:/Program Files/OpenSSL-Win64/bin")
        if(EXISTS "${OPENSSL_BIN_DIR}/libssl-1_1-x64.dll")
            add_custom_command(TARGET TradingTerminal POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OPENSSL_BIN_DIR}/libssl-1_1-x64.dll"
                    "${OPENSSL_BIN_DIR}/libcrypto-1_1-x64.dll"
                    "$<TARGET_FILE_DIR:TradingTerminal>"
                COMMENT "Deploying OpenSSL DLLs..."
            )
        endif()
    endif()

    # Copy MinGW runtime DLLs
    if(MINGW)
        set(CHOCOLATEY_MINGW_BIN "C:/ProgramData/chocolatey/lib/mingw/tools/install/mingw64/bin")
        if(EXISTS "${CHOCOLATEY_MINGW_BIN}")
            add_custom_command(TARGET TradingTerminal POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CHOCOLATEY_MINGW_BIN}/libgcc_s_seh-1.dll"
                    "${CHOCOLATEY_MINGW_BIN}/libstdc++-6.dll"
                    "${CHOCOLATEY_MINGW_BIN}/libwinpthread-1.dll"
                    "$<TARGET_FILE_DIR:TradingTerminal>"
                COMMENT "Copying MinGW runtime DLLs from Chocolatey..."
            )
        else()
            get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            file(GLOB MINGW_RUNTIME_DLLS
                "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
                "${MINGW_BIN_DIR}/libstdc++-6.dll"
                "${MINGW_BIN_DIR}/libwinpthread-1.dll"
            )
            if(MINGW_RUNTIME_DLLS)
                add_custom_command(TARGET TradingTerminal POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_RUNTIME_DLLS} "$<TARGET_FILE_DIR:TradingTerminal>"
                    COMMENT "Copying MinGW runtime DLLs..."
                )
            else()
                message(WARNING "MinGW runtime DLLs not found.")
            endif()
        endif()
    endif()

    # Copy TradingView library files
    if(ENABLE_TRADINGVIEW AND Qt5WebEngineWidgets_FOUND AND Qt5WebChannel_FOUND)
        add_custom_command(TARGET TradingTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TradingTerminal>/resources"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources/tradingview"
                "$<TARGET_FILE_DIR:TradingTerminal>/resources/tradingview"
            COMMENT "Copying TradingView library files to build directory..."
        )
    endif()
endif()
