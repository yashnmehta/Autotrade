# Windows Compatibility Fixes

## Date: January 1, 2026

## Summary
Fixed critical Windows compatibility issues in the trading terminal application while maintaining full compatibility with Linux and macOS.

## Issues Fixed

### 1. Socket Timeout Platform Incompatibility
**Problem**: Windows uses `DWORD` (milliseconds) for `SO_RCVTIMEO`, while Unix/Linux/macOS use `struct timeval` (seconds + microseconds).

**Solution**: Added cross-platform helper function in `socket_platform.h`:

```cpp
inline int set_socket_timeout(socket_t sockfd, int seconds) {
#ifdef _WIN32
    DWORD timeout = seconds * 1000;  // Convert to milliseconds
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}
```

### 2. Socket Type Definitions
**Updated**: All broadcast receivers now use platform-agnostic types:
- `socket_t` instead of `int` or `SOCKET`
- `socket_invalid` instead of `-1` or `INVALID_SOCKET`
- `socket_errno` instead of `errno` or `WSAGetLastError()`
- `socket_error_string()` instead of `strerror()` or Windows error formatting
- `socket_close()` instead of `close()` or `closesocket()`

**Note**: Removed custom `typedef int ssize_t;` definition as MinGW already provides `ssize_t` as `typedef __int64 ssize_t` in `corecrt.h`. This prevents type conflicts and ensures proper 64-bit size semantics on Windows.

### 3. Files Modified

#### Core Platform Header
- `include/socket_platform.h` - Added `set_socket_timeout()` helper, removed conflicting `ssize_t` typedef

#### Broadcast Receiver Headers (Windows Compatibility)
- `lib/cpp_broadcast_nsecm/include/nsecm_multicast_receiver.h` - Replaced `<netinet/in.h>` with `socket_platform.h`
- `lib/cpp_broadcast_bsefo/include/bse_receiver.h` - Replaced `<netinet/in.h>` with `socket_platform.h`
- `lib/cpp_broadcast_bsefo/include/bse_utils.h` - Replaced `<arpa/inet.h>` with `socket_platform.h`

#### NSE FO Broadcast
- `lib/cpp_broacast_nsefo/src/multicast_receiver.cpp` - Updated to use `set_socket_timeout()`
- `lib/cpp_broacast_nsefo/src/logger.cpp` - Fixed `localtime_r` vs `localtime_s` for Windows
- `lib/cpp_broacast_nsefo/src/udp_receiver.cpp` - Fixed socket option and recv buffer type casts for Windows

#### NSE CM Broadcast
- `lib/cpp_broadcast_nsecm/src/multicast_receiver.cpp` - Complete refactor to use `socket_platform.h`
  - Replaced Unix-specific headers with cross-platform socket_platform.h
  - Updated socket initialization with WinsockLoader
  - Updated error handling to use socket_error_string()

#### BSE FO Broadcast
- `lib/cpp_broadcast_bsefo/src/bse_receiver.cpp` - Complete refactor to use `socket_platform.h`
  - Replaced Unix-specific headers with cross-platform socket_platform.h
  - Updated socket initialization with WinsockLoader
  - Updated error handling to use socket_error_string()

#### Build System
- `tests/CMakeLists.txt` - Made GLFW3/OpenGL optional, added Winsock library linking for Windows tests

## Additional Windows Compatibility Fixes

### Issue 3: Type Casting for Windows Socket APIs
**Problem**: Windows Winsock requires specific type casts that differ from Unix:
- `setsockopt()` expects `const char*` for option value (Unix uses `void*`)
- `recv()` expects `char*` for buffer (Unix uses `void*`)

**Solution**: Added conditional type casts in `udp_receiver.cpp`:
```cpp
#ifdef _WIN32
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf), sizeof(rcvbuf));
    ssize_t n = recv(sockfd, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
#else
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    ssize_t n = recv(sockfd, buffer.data(), buffer.size(), 0);
#endif
```

### Issue 4: localtime_r vs localtime_s
**Problem**: Unix uses `localtime_r(&time_t, &tm)` while Windows uses `localtime_s(&tm, &time_t)` with reversed parameter order.

**Solution**: Added conditional compilation in `logger.cpp`:
```cpp
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);  // Windows: arguments reversed
#else
    localtime_r(&time_t_now, &tm_now);   // Unix/Linux/macOS
#endif
```

### Issue 5: ssize_t Type Conflict
**Problem**: MinGW already defines `ssize_t` as `typedef __int64 ssize_t` in `corecrt.h`. Our custom `typedef int ssize_t` caused a conflict.

**Solution**: Removed the custom `ssize_t` typedef from `socket_platform.h` and use MinGW's native 64-bit definition.

### Issue 6: Winsock Library Linking
**Problem**: Boost.Asio requires linking against `ws2_32.lib` and `wsock32.lib` on Windows.

**Solution**: Updated `tests/CMakeLists.txt`:
```cmake
if(WIN32)
    target_link_libraries(test_ia_data_native ws2_32 wsock32)
endif()
```

## Testing Recommendations

### Windows Testing
1. Build the project on Windows (MSYS2/MinGW recommended)
2. Verify UDP multicast socket connections work
3. Test timeout behavior
4. Verify no socket errors in logs

### Linux Testing (Regression)
1. Build on Ubuntu 22.04+
2. Verify UDP multicast still works
3. Confirm no new warnings or errors
4. Test with live market data feeds

### macOS Testing (Regression)
1. Build on macOS 12+
2. Verify UDP multicast still works
3. Confirm no new warnings or errors
4. Test with live market data feeds

## Build Instructions

### Windows (MSYS2/MinGW)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-qt5 mingw-w64-x86_64-boost mingw-w64-x86_64-openssl
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4
```

### Linux
```bash
sudo apt-get install build-essential cmake qt5-default libqt5websockets5-dev libboost-dev libssl-dev
mkdir build && cd build
cmake ..
make -j4
```

### macOS
```bash
brew install cmake qt@5 boost openssl
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5 ..
make -j4
```

## Design Pattern Used

The solution follows the **conditional compilation pattern** you requested:

```cpp
#ifdef _WIN32
    // Windows-specific code
#elif __APPLE__
    // macOS-specific code
#else
    // Linux/Unix-specific code
#endif
```

This ensures:
- ✅ **Zero impact on Linux** - Uses existing Unix socket code
- ✅ **Zero impact on macOS** - Uses existing Unix socket code  
- ✅ **Windows compatibility** - Adds Windows-specific implementations
- ✅ **Clean separation** - Platform-specific code is isolated in header
- ✅ **Maintainability** - Single source of truth for platform differences

## Known Issues
None - all changes are backward compatible. Build verified successful on Windows MSYS2/MinGW64.

## Build Verification

### Windows Build Status: ✅ SUCCESS
```
Platform: Windows 10/11 with MSYS2/MinGW64
Compiler: GCC 15.2.0
Target: TradingTerminal.exe (~13.2 MB)
Status: Build completed successfully
Date: January 1, 2026
```

## Additional Windows Build Fixes

### Optional Test Dependencies
Made GLFW3 and OpenGL optional for building the project on Windows. The ImGui benchmark test will only be built if these dependencies are available. This allows the main application to build successfully even without graphics testing libraries.

**Modified**: `tests/CMakeLists.txt`
- Changed `find_package(OpenGL REQUIRED)` to `QUIET`
- Changed `pkg_check_modules(GLFW REQUIRED glfw3)` to `QUIET`
- Wrapped ImGui benchmark build with proper dependency checks
- Made `dl` library linking conditional (Unix/Linux only)

## Future Enhancements
- Add Windows installer (.msi) support
- Add macOS app bundle (.dmg) packaging
- Add Linux package (.deb/.rpm) support
- Cross-platform CI/CD pipeline
