# OpenSSL Configuration Fix

## Problem

CMake cannot find OpenSSL even though MSYS2 MinGW64 is installed:
```
CMake Error: Could NOT find OpenSSL, try to set the path to OpenSSL root folder
(missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)
```

## Solution

### Quick Fix (Run This Script)

```powershell
.\configure-openssl.ps1
```

This script will:
1. ✅ Verify MSYS2 MinGW64 is installed
2. ✅ Verify OpenSSL headers exist
3. ✅ Verify OpenSSL libraries exist
4. ✅ Clear CMake cache
5. ✅ Run CMake configuration

---

## Manual Fix

### Step 1: Install OpenSSL (if not installed)

Open **MSYS2 MinGW 64-bit** terminal:
```bash
pacman -S mingw-w64-x86_64-openssl
```

### Step 2: Verify Installation

Check that these files exist:
```
C:\msys64\mingw64\include\openssl\ssl.h
C:\msys64\mingw64\lib\libcrypto.dll.a
C:\msys64\mingw64\lib\libssl.dll.a
```

### Step 3: Clear CMake Cache

```powershell
cd build
Remove-Item CMakeCache.txt -Force
Remove-Item CMakeFiles -Recurse -Force
```

### Step 4: Run CMake

```powershell
cmake ..
```

---

## What Was Changed in CMakeLists.txt

### Before (Broken)
```cmake
if(MINGW AND EXISTS "C:/msys64/mingw64")
    set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
    set(OPENSSL_INCLUDE_DIR "C:/msys64/mingw64/include")
    set(OPENSSL_CRYPTO_LIBRARY "C:/msys64/mingw64/lib/libcrypto.dll.a")
    set(OPENSSL_SSL_LIBRARY "C:/msys64/mingw64/lib/libssl.dll.a")
endif()

find_package(OpenSSL REQUIRED)  # ❌ Ignores manual paths
```

**Problem**: `find_package(OpenSSL)` ignores the manually set variables and searches in default locations.

### After (Fixed)
```cmake
if(MINGW AND EXISTS "C:/msys64/mingw64")
    # Set paths with CACHE FORCE to override find_package
    set(OPENSSL_ROOT_DIR "C:/msys64/mingw64" CACHE PATH "OpenSSL root directory" FORCE)
    set(OPENSSL_INCLUDE_DIR "C:/msys64/mingw64/include" CACHE PATH "OpenSSL include directory" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "C:/msys64/mingw64/lib/libcrypto.dll.a" CACHE FILEPATH "OpenSSL crypto library" FORCE)
    set(OPENSSL_SSL_LIBRARY "C:/msys64/mingw64/lib/libssl.dll.a" CACHE FILEPATH "OpenSSL SSL library" FORCE)
    
    # Verify files exist
    if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
        message(FATAL_ERROR "OpenSSL headers not found")
    endif()
    
    # Set found flag
    set(OPENSSL_FOUND TRUE CACHE BOOL "OpenSSL found" FORCE)
    
    # Create imported targets (required for target_link_libraries)
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

# Only call find_package if OpenSSL wasn't manually configured
if(NOT OPENSSL_FOUND)
    find_package(OpenSSL REQUIRED)
endif()
```

**Benefits**:
- ✅ Forces CMake to use MSYS2 OpenSSL paths
- ✅ Verifies files exist before proceeding
- ✅ Creates imported targets manually
- ✅ Skips `find_package` if already configured
- ✅ Provides helpful error messages

---

## Troubleshooting

### Error: "OpenSSL headers not found"

**Solution**: Install OpenSSL in MSYS2:
```bash
# Open MSYS2 MinGW 64-bit terminal
pacman -S mingw-w64-x86_64-openssl
```

### Error: "MSYS2 MinGW64 not found at C:/msys64/mingw64"

**Solution**: Install MSYS2 from https://www.msys2.org/

### CMake still can't find OpenSSL

**Solution**: Delete entire build directory and reconfigure:
```powershell
Remove-Item build -Recurse -Force
New-Item -ItemType Directory -Path build
cd build
cmake ..
```

### Error: "libssl-3-x64.dll not found" (at runtime)

**Solution**: Copy OpenSSL DLLs to build directory:
```powershell
Copy-Item "C:\msys64\mingw64\bin\libssl-3-x64.dll" -Destination "build\"
Copy-Item "C:\msys64\mingw64\bin\libcrypto-3-x64.dll" -Destination "build\"
```

---

## Verification

After running CMake, you should see:
```
-- Detected MSYS2 MinGW64 environment
-- Using MSYS2 OpenSSL 3.x
--   Include dir: C:/msys64/mingw64/include
--   Crypto library: C:/msys64/mingw64/lib/libcrypto.dll.a
--   SSL library: C:/msys64/mingw64/lib/libssl.dll.a
-- Configuring done
-- Generating done
-- Build files have been written to: D:/Pratham/Terminal/trading_terminal_cpp/build
```

---

## Expected Folder Structure

```
C:\msys64\
└── mingw64\
    ├── bin\
    │   ├── libcrypto-3-x64.dll    # Runtime (needed at runtime)
    │   └── libssl-3-x64.dll       # Runtime (needed at runtime)
    ├── include\
    │   └── openssl\
    │       ├── ssl.h              # Headers (needed at compile time)
    │       └── crypto.h
    └── lib\
        ├── libcrypto.dll.a        # Import library (needed at link time)
        └── libssl.dll.a           # Import library (needed at link time)
```

---

## Summary

**What was broken**: CMake's `find_package(OpenSSL)` was ignoring manually set paths.

**What was fixed**: 
1. Use `CACHE FORCE` to override CMake's search
2. Create imported targets manually
3. Verify files exist before proceeding
4. Skip `find_package` if already configured

**Result**: ✅ CMake successfully finds MSYS2 OpenSSL 3.x

---

**Date**: January 13, 2026  
**Status**: ✅ **FIXED**
