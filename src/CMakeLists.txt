# ========================================
# MAIN EXECUTABLE - TradingTerminal
# Links all per-module static libraries
# ========================================

# Add per-module subdirectories (each builds a static library)
add_subdirectory(core)
add_subdirectory(api)
add_subdirectory(services)
add_subdirectory(repository)
add_subdirectory(quant)
add_subdirectory(models)
add_subdirectory(strategy)
add_subdirectory(views)
add_subdirectory(ui)
add_subdirectory(data)
add_subdirectory(search)
add_subdirectory(indicators)
add_subdirectory(utils)

# ========================================
# APP LAYER - Main entry + bootstrap + factories
# ========================================
set(APP_SOURCES
    main.cpp
    app/AppBootstrap.cpp
    app/WindowFactory.cpp
    app/WorkspaceManager.cpp
    app/MainWindow/MainWindow.cpp
    app/MainWindow/UI.cpp
    app/MainWindow/Windows.cpp
    app/MainWindow/Network.cpp
    app/MainWindow/WindowCycling.cpp
    app/ScripBar.cpp
    app/SnapQuoteScripBar.cpp
)

set(APP_HEADERS
    ${CMAKE_SOURCE_DIR}/include/app/AppBootstrap.h
    ${CMAKE_SOURCE_DIR}/include/app/MainWindow.h
    ${CMAKE_SOURCE_DIR}/include/app/WindowFactory.h
    ${CMAKE_SOURCE_DIR}/include/app/WorkspaceManager.h
    ${CMAKE_SOURCE_DIR}/include/app/ScripBar.h
    ${CMAKE_SOURCE_DIR}/include/app/SnapQuoteScripBar.h
)

# ========================================
# UDP Header definitions (header-only, no .cpp)
# ========================================
set(UDP_HEADERS
    ${CMAKE_SOURCE_DIR}/include/udp/UDPTypes.h
    ${CMAKE_SOURCE_DIR}/include/udp/UDPEnums.h
)

# ========================================
# BROADCAST RECEIVER SOURCES
# NSE FO/CM are compiled directly into the exe (not as sub-libs)
# because they depend on the parent include tree and Qt5
# ========================================
set(NSEFO_BROADCAST_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/nsefo_price_store.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/multicast_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/udp_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/config.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/utils/parse_compressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/utils/parse_uncompressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7200.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7202.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7203.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7207.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7208.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7211.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_7220.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_17201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_17202.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsefo/src/parser/parse_message_rest.cpp
)

set(NSECM_BROADCAST_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/nsecm_price_store.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/multicast_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/udp_receiver.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/nse_parsers.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/config.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/utils/parse_compressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/utils/parse_uncompressed_message.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_5295.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6013.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7200.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7201.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7208.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_18703.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_18708.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7207.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7203.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6501.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7206.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_7210.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6511.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6521.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6522.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6531.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6541.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_6571.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_9010.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_nsecm/src/parser/parse_message_9011.cpp
)

set(BSEFO_BROADCAST_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/src/bse_price_store.cpp
    ${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsefo/src/bse_receiver.cpp
)

# ========================================
# UI FORMS (.ui files)
# ========================================
set(FORMS
    ${CMAKE_SOURCE_DIR}/resources/forms/InfoBar.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/SplashScreen.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/LoginWindow.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/customize.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesWindowTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesGeneralTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesOrderTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesDerivativeTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesAlertMessageTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesMarginPlusOrderTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesWorkSpaceTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/PreferencesPortfolioTab.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/strategy/StrategyManagerWindow.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/strategy/CreateStrategyDialog.ui
    ${CMAKE_SOURCE_DIR}/resources/forms/strategy/StrategyTemplateBuilderDialog.ui
)

# ========================================
# RESOURCE FILES
# ========================================
set(RESOURCES
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)

# ========================================
# BUILD THE EXECUTABLE
# ========================================
add_executable(TradingTerminal
    ${APP_SOURCES}
    ${APP_HEADERS}
    ${UDP_HEADERS}
    ${FORMS}
    ${RESOURCES}
    ${NSEFO_BROADCAST_SOURCES}
    ${NSECM_BROADCAST_SOURCES}
    ${BSEFO_BROADCAST_SOURCES}
)

# Fix MSVC PDB contention - use embedded debug info
if(MSVC)
    target_compile_options(TradingTerminal PRIVATE
        /Z7
        $<$<CONFIG:Debug>:/Od>
    )
endif()

# ========================================
# LINK ALL MODULE LIBRARIES
# ========================================
target_link_libraries(TradingTerminal PRIVATE
    # Per-module static libraries
    core_widgets
    api_layer
    services
    repository
    quant
    models
    strategy_engine
    views
    ui_dialogs
    data_layer
    search
    indicators
    utils
    common_lzo

    # Qt5 modules
    Qt5::Widgets
    Qt5::Network
    Qt5::WebSockets
    Qt5::UiTools
    Qt5::Concurrent
    Qt5::Sql

    # System libraries
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${LZO_LIBRARIES}

    # Broadcast libraries
    cpp_broadcast_bsefo
)

# Add TradingView dependencies if enabled
if(ENABLE_TRADINGVIEW AND Qt5WebEngineWidgets_FOUND AND Qt5WebChannel_FOUND)
    target_link_libraries(TradingTerminal PRIVATE
        Qt5::WebEngineWidgets
        Qt5::WebChannel
    )
endif()

# Add QtCharts if enabled
if(ENABLE_QTCHARTS AND Qt5Charts_FOUND)
    target_link_libraries(TradingTerminal PRIVATE Qt5::Charts)
endif()

# Add TA-Lib if found
if(TALIB_FOUND)
    target_link_libraries(TradingTerminal PRIVATE ${TALIB_LIBRARY})
    target_include_directories(TradingTerminal PRIVATE ${TALIB_INCLUDE_DIR})
    target_compile_definitions(TradingTerminal PRIVATE HAVE_TALIB)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(TradingTerminal PRIVATE Ws2_32 Iphlpapi Psapi)
    if(MINGW)
        target_link_libraries(TradingTerminal PRIVATE winpthread)
        target_link_options(TradingTerminal PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# Boost headers (Beast is header-only)
target_include_directories(TradingTerminal PRIVATE ${Boost_INCLUDE_DIRS})

# macOS Bundle Settings
if(APPLE)
    set_target_properties(TradingTerminal PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "TradingTerminal"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.trading.terminal"
        MACOSX_BUNDLE_INFO_STRING "Professional Trading Terminal"
    )
endif()

# ========================================
# POST-BUILD: Windows DLL Deployment
# (Must be in the same directory as add_executable)
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
