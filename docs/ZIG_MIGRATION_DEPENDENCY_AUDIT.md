# Zig Migration Dependency Audit - Trading Terminal C++

**Date**: February 5, 2026  
**Codebase**: d:\trading_terminal_cpp  
**Current Stack**: C++17, CMake 3.14+, Qt5, Boost, OpenSSL, LZO

---

## Executive Summary

This audit catalogs all external dependencies for migration from C++ to Zig. The codebase is a professional trading terminal with:
- Real-time UDP multicast market data receivers (NSE/BSE)
- WebSocket/REST API clients for XTS trading platform
- Qt5-based rich desktop UI with custom widgets
- High-performance data processing pipeline

**Migration Complexity**: HIGH - Deep Qt5 integration, template-heavy Boost usage

---

## 1. Comprehensive Dependency Matrix

### 1.1 Build System

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| CMake | 3.14+ | Build configuration, Qt MOC/UIC/RCC | Zig build system (`build.zig`) | **Medium** - Zig build is simpler but needs Qt toolchain integration |
| Make/Ninja | N/A | Build execution | Built into `zig build` | **Easy** |
| pkg-config | N/A | Dependency discovery | `@cImport` for C libs, manual for Qt | **Easy** |
| windeployqt | Qt5 | Windows DLL deployment | Manual or custom script | **Medium** |

### 1.2 UI Framework - Qt5 (CRITICAL)

| Component | Module | Purpose | Zig Alternative | Migration Difficulty |
|-----------|--------|---------|-----------------|---------------------|
| Qt5::Widgets | Core | QMainWindow, QTableView, QMDIArea, custom widgets | **None direct** - Options: raylib-zig, SDL2-zig, mach-imgui, or keep C++ | **VERY HARD** |
| Qt5::Network | Networking | QNetworkAccessManager (being phased out) | Zig std.net, zig-network | **Medium** (already using Boost.Beast) |
| Qt5::WebSockets | WebSocket | QWebSocket (legacy, replaced by Boost.Beast) | `std.net.Stream` + WebSocket protocol | **Medium** |
| Qt5::UiTools | UI Loader | Runtime .ui file loading | Not applicable | **N/A** |
| Qt5::Concurrent | Threading | QtConcurrent::run, QFuture | Zig async/await, std.Thread | **Medium** |
| Qt MOC | Meta-Object | signals/slots, Q_OBJECT macro | Event callbacks, observer pattern | **Hard** |
| Qt AUTOUIC | UI Files | .ui → C++ header generation | Manual UI code or different framework | **Hard** |
| Qt Resources | Assets | .qrc resource compilation | `@embedFile`, std.fs | **Easy** |

**Qt Components Used in Code:**
```
QApplication, QMainWindow, QMDIArea, QMDISubWindow, QWidget
QTableView, QTreeView, QListView, QComboBox, QLineEdit
QVBoxLayout, QHBoxLayout, QGridLayout, QSplitter
QPushButton, QLabel, QCheckBox, QSpinBox
QMenu, QMenuBar, QToolBar, QStatusBar, QDockWidget
QTimer, QElapsedTimer, QDateTime
QJsonDocument, QJsonObject, QJsonArray
QFile, QDir, QSettings, QStandardPaths
QString, QVector, QMap, QSet, QHash
QThread, QMutex, QWaitCondition
QDebug, qRegisterMetaType
```

### 1.3 Networking Libraries

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| Boost.Beast | 1.70+ | HTTP/WebSocket (header-only) | `std.net` + custom WebSocket impl, or `zig-network` | **Hard** - Beast is template-heavy |
| Boost.Asio | 1.70+ | Async I/O, TCP/SSL | `std.net.Stream`, `std.io.poll` | **Hard** - Different async model |
| Native Sockets | POSIX/Win | UDP multicast receivers | `std.os.socket`, `std.os.system` | **Easy** - Zig has good socket support |
| Winsock2 | Win32 | Windows networking | Zig std handles this cross-platform | **Easy** |

**Network Implementation Details:**
- `NativeHTTPClient.cpp` - Uses Boost.Beast for HTTP/HTTPS
- `NativeWebSocketClient.cpp` - Uses Boost.Beast for WebSocket + TLS
- `MulticastReceiver` - Pure POSIX/Winsock UDP sockets (easier to port)
- Custom Socket.IO/Engine.IO protocol implementation

### 1.4 TLS/Cryptography

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| OpenSSL | 1.1.1+ | TLS 1.2+ for WebSocket, HTTPS | **Options**: 1. BearSSL (small, C) 2. mbed TLS (C) 3. `@cImport("openssl")` | **Medium** |
| SSL_CTX | OpenSSL | SSL context management | BearSSL br_ssl_client_context | **Medium** |
| SSL_set_tlsext_host_name | OpenSSL | SNI hostname setting | BearSSL equivalent | **Easy** |

**Current SSL Usage:**
```cpp
ssl::context sslCtx{ssl::context::tlsv12_client};
sslCtx.set_verify_mode(ssl::verify_none);  // Ignoring cert errors
SSL_set_tlsext_host_name(handle, host.c_str());  // SNI
```

### 1.5 Compression

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| LZO | 2.10 (bundled) | Market data decompression (LZO1Z) | 1. `@cImport("lzo")` (easiest) 2. Pure Zig port 3. Alternative: std.compress.zlib | **Easy** |

**LZO Details:**
- Bundled at `lib/lzo-2.10/` (C library, no C++)
- Custom reimplementation at `lib/common/src/lzo_decompress.cpp`
- Used for NSE/BSE compressed market data packets
- **Recommendation**: Use `@cImport` to wrap existing LZO C code

### 1.6 JSON Processing

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| Qt JSON | Qt5 | REST API parsing, config files | `std.json` (built-in) | **Easy** |

**Current Usage:**
- `QJsonDocument::fromJson()` for HTTP responses
- `QJsonObject/QJsonArray` for data extraction
- Config file parsing

### 1.7 Concurrency & Threading

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| QThread | Qt5 | Worker threads (MasterLoaderWorker) | `std.Thread` | **Easy** |
| QMutex | Qt5 | Thread synchronization | `std.Thread.Mutex` | **Easy** |
| QWaitCondition | Qt5 | Thread coordination | `std.Thread.Condition` | **Easy** |
| std::thread | C++11 | Native threads | `std.Thread` | **Easy** |
| std::mutex | C++11 | Native mutexes | `std.Thread.Mutex` | **Easy** |
| std::atomic | C++11 | Lock-free primitives | `std.atomic.Atomic` | **Easy** |
| std::shared_mutex | C++17 | Reader-writer locks | `std.Thread.RwLock` | **Easy** |
| std::condition_variable | C++11 | Thread signaling | `std.Thread.Condition` | **Easy** |

### 1.8 Mathematical Functions

| Dependency | Version | Purpose | Zig Alternative | Migration Difficulty |
|------------|---------|---------|-----------------|---------------------|
| std::cmath | C++11 | Black-Scholes, Greeks, IV calculation | `std.math` | **Easy** |
| std::algorithm | C++11 | Sorting, searching | Zig slices with sort/search | **Easy** |

**Math-Heavy Components:**
- `Greeks.h/cpp` - Black-Scholes option pricing
- `IVCalculator.h/cpp` - Newton-Raphson IV solver
- All use standard `exp()`, `log()`, `sqrt()`, `erf()` - available in `std.math`

### 1.9 Platform Abstraction

| Platform Feature | C++ Implementation | Zig Alternative | Migration Difficulty |
|-----------------|-------------------|-----------------|---------------------|
| Windows Sockets | Winsock2, Ws2_32.lib | `std.os.windows` | **Easy** |
| POSIX Sockets | sys/socket.h | `std.os.socket` | **Easy** |
| Memory Profiling | Windows PSAPI | Custom or skip | **Medium** |
| File System | QFile, QDir, std::filesystem | `std.fs` | **Easy** |
| High-DPI Scaling | Qt AA_EnableHighDpiScaling | Framework-dependent | **Medium** |
| Sound/Beep | QSound / Windows API | Platform-specific or skip | **Low Priority** |

### 1.10 Custom/Bundled Libraries

| Library | Path | Purpose | Zig Migration Strategy |
|---------|------|---------|----------------------|
| cpp_broacast_nsefo | lib/cpp_broacast_nsefo/ | NSE F&O UDP receiver | Pure Zig rewrite (no external deps) |
| cpp_broadcast_nsecm | lib/cpp_broadcast_nsecm/ | NSE CM UDP receiver | Pure Zig rewrite |
| cpp_broadcast_bsefo | lib/cpp_broadcast_bsefo/ | BSE F&O/CM UDP receiver | Pure Zig rewrite |
| common | lib/common/ | LZO decompression utils | Wrap with @cImport or port |
| imgui | lib/imgui/ | (Empty - unused) | Could use mach-imgui if needed |

---

## 2. Header-Only Libraries (Easy to Wrap)

These can potentially be used via `@cImport` if needed:

| Library | Type | Notes |
|---------|------|-------|
| Boost.Beast | C++ Header-only | Template-heavy, **cannot** use @cImport directly |
| Boost.Asio | C++ Header-only | Template-heavy, **cannot** use @cImport directly |
| LZO headers | C | ✅ Good candidate for @cImport |

---

## 3. C Libraries (Good @cImport Candidates)

| Library | Files | @cImport Feasibility |
|---------|-------|---------------------|
| LZO 2.10 | `lib/lzo-2.10/` | ✅ Excellent - Pure C |
| OpenSSL | System | ✅ Good - C API |
| Winsock2 | System | ✅ Already supported by Zig std |
| POSIX sockets | System | ✅ Already supported by Zig std |

**Example @cImport for LZO:**
```zig
const lzo = @cImport({
    @cInclude("lzo/lzo1z.h");
    @cInclude("lzo/lzoconf.h");
});

pub fn decompress(src: []const u8, dst: []u8) !usize {
    var dst_len: c_ulong = dst.len;
    const result = lzo.lzo1z_decompress(src.ptr, src.len, dst.ptr, &dst_len, null);
    if (result != lzo.LZO_E_OK) return error.DecompressionFailed;
    return dst_len;
}
```

---

## 4. C++ Template-Heavy Libraries (Hard to Interop)

| Library | Difficulty | Reason |
|---------|-----------|--------|
| Boost.Beast | **Very Hard** | Heavy template metaprogramming, SFINAE |
| Boost.Asio | **Very Hard** | Proactor pattern with templates |
| Qt5 | **Very Hard** | MOC-generated code, signals/slots, QObject inheritance |
| STL containers | **Hard** | Template instantiation required |

**Recommendation**: Rewrite these parts in pure Zig rather than trying to interop.

---

## 5. Migration Path Recommendations

### Phase 1: Low-Hanging Fruit (Easy)
1. **LZO Decompression** - Wrap with @cImport
2. **UDP Receivers** - Pure Zig rewrite (simple socket code)
3. **Math Libraries** - Direct port using std.math
4. **JSON Parsing** - Use std.json
5. **Threading** - Use std.Thread

### Phase 2: Networking (Medium)
1. **HTTP Client** - Rewrite using std.net + TLS
2. **WebSocket Client** - Implement WebSocket protocol in Zig
3. **TLS** - Use BearSSL or mbed TLS via @cImport

### Phase 3: UI Framework (Hard - Major Decision Required)

**Options:**

| Option | Pros | Cons |
|--------|------|------|
| **A. Keep Qt5 via C FFI** | Minimal rewrite, proven UI | Complex interop, still need Qt |
| **B. raylib-zig** | Immediate mode, game-like | Limited widgets, not traditional desktop |
| **C. SDL2 + custom** | Cross-platform, C interop | Build everything from scratch |
| **D. mach-imgui** | Fast, immediate mode | Not traditional desktop look |
| **E. Gtk via @cImport** | Traditional desktop | Complex C API, Linux-focused |
| **F. Hybrid** | Keep C++ UI, Zig backend | Manageable, incremental | FFI overhead |

**Recommended Approach: Option F (Hybrid)**
1. Keep Qt5 C++ for UI layer
2. Rewrite backend services in Zig
3. Use C FFI bridge between Qt and Zig
4. Gradually migrate UI components if needed

---

## 6. Estimated Migration Effort

| Component | Lines of Code | Effort (weeks) | Priority |
|-----------|--------------|----------------|----------|
| UDP Receivers (NSE/BSE) | ~3,000 | 2 | High |
| LZO Decompression | ~500 | 0.5 | High |
| HTTP/WebSocket Client | ~1,500 | 3 | High |
| Price Store/Data Layer | ~2,000 | 2 | High |
| Greeks/IV Calculator | ~500 | 1 | Medium |
| Repository Layer | ~3,000 | 2 | Medium |
| Qt UI Layer | ~15,000 | 12+ | Low (defer) |
| **Total Backend** | ~10,500 | ~10.5 weeks | |

---

## 7. Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Qt5 interop complexity | High | Use hybrid approach, don't migrate UI |
| Boost.Beast replacement | Medium | Test thoroughly, may need custom WebSocket |
| OpenSSL → BearSSL | Low | BearSSL is well-tested, similar API |
| Performance regression | Medium | Benchmark critical paths |
| UDP multicast timing | High | Zig's low-level control should help |

---

## 8. File Summary

### Key Source Files to Migrate

**Networking (High Priority):**
- [src/api/NativeHTTPClient.cpp](src/api/NativeHTTPClient.cpp) - Boost.Beast HTTP
- [src/api/NativeWebSocketClient.cpp](src/api/NativeWebSocketClient.cpp) - Boost.Beast WebSocket
- [src/services/UdpBroadcastService.cpp](src/services/UdpBroadcastService.cpp) - UDP coordinator

**Data Processing (High Priority):**
- [lib/cpp_broacast_nsefo/src/](lib/cpp_broacast_nsefo/src/) - NSE FO UDP receiver (~10 files)
- [lib/cpp_broadcast_nsecm/src/](lib/cpp_broadcast_nsecm/src/) - NSE CM UDP receiver (~15 files)
- [lib/cpp_broadcast_bsefo/src/](lib/cpp_broadcast_bsefo/src/) - BSE receiver (~5 files)
- [lib/common/src/lzo_decompress.cpp](lib/common/src/lzo_decompress.cpp) - LZO utils

**Math/Finance (Medium Priority):**
- [src/repository/Greeks.cpp](src/repository/Greeks.cpp) - Black-Scholes implementation
- [src/repository/IVCalculator.cpp](src/repository/IVCalculator.cpp) - IV calculation

**Qt UI (Defer):**
- [src/core/widgets/](src/core/widgets/) - Custom Qt widgets (11 files)
- [src/views/](src/views/) - Trading windows (20+ files)
- [src/app/MainWindow/](src/app/MainWindow/) - Main application window

---

## 9. Conclusion

The trading terminal has **deep Qt5 integration** making a full Zig migration challenging. The recommended approach is:

1. **Start with backend/data layer** - UDP receivers, data processing, calculations
2. **Keep Qt5 UI** - Use C FFI to call Zig backend from C++
3. **Replace Boost with pure Zig** - Biggest win for code simplification
4. **Evaluate UI migration later** - After backend is stable

This hybrid approach minimizes risk while gaining Zig's benefits for performance-critical code paths.// 