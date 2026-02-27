# ========================================
# UTILS LAYER - Helper Utilities
# ========================================

add_library(utils STATIC
    ClipboardHelpers.cpp
    SelectionHelpers.cpp
    ConfigLoader.cpp
    DateUtils.cpp
    MemoryProfiler.cpp
    PreferencesManager.cpp
    SoundManager.cpp
    WindowManager.cpp
    LicenseManager.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/utils/BookWindowHelper.h
    ${CMAKE_SOURCE_DIR}/include/utils/ClipboardHelpers.h
    ${CMAKE_SOURCE_DIR}/include/utils/ConfigLoader.h
    ${CMAKE_SOURCE_DIR}/include/utils/DateUtils.h
    ${CMAKE_SOURCE_DIR}/include/utils/FileLogger.h
    ${CMAKE_SOURCE_DIR}/include/utils/LatencyTracker.h
    ${CMAKE_SOURCE_DIR}/include/utils/LicenseManager.h
    ${CMAKE_SOURCE_DIR}/include/utils/LockFreeQueue.h
    ${CMAKE_SOURCE_DIR}/include/utils/MemoryProfiler.h
    ${CMAKE_SOURCE_DIR}/include/utils/PreferencesManager.h
    ${CMAKE_SOURCE_DIR}/include/utils/SelectionHelpers.h
    ${CMAKE_SOURCE_DIR}/include/utils/SoundManager.h
    ${CMAKE_SOURCE_DIR}/include/utils/TableProfileHelper.h
    ${CMAKE_SOURCE_DIR}/include/utils/WindowManager.h
    ${CMAKE_SOURCE_DIR}/include/utils/WindowSettingsHelper.h
)

target_link_libraries(utils PUBLIC
    Qt5::Widgets
    Qt5::Network
)
