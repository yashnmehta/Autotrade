# TA-Lib Configuration
set(TALIB_ROOT "C:/ta-lib/c" CACHE PATH "TA-Lib root directory")
set(TALIB_INCLUDE_DIR "${TALIB_ROOT}/include")
set(TALIB_LIBRARY_RELEASE "${TALIB_ROOT}/lib/ta_libc_cdr.lib")
set(TALIB_LIBRARY_DEBUG "${TALIB_ROOT}/lib/ta_libc_cmd.lib")

if(EXISTS "${TALIB_LIBRARY_RELEASE}")
    set(TALIB_FOUND TRUE)
    set(TALIB_LIBRARY debug "${TALIB_LIBRARY_DEBUG}" optimized "${TALIB_LIBRARY_RELEASE}")
    message(STATUS "Found TA-Lib: ${TALIB_ROOT}")
else()
    set(TALIB_FOUND FALSE)
    message(WARNING "TA-Lib NOT found at ${TALIB_ROOT}. Technical indicators will be disabled.")
endif()
