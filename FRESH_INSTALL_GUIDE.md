# Fresh Qt Installation Guide

## Overview
Complete fresh installation of Qt 5.15.2 with MinGW using Qt Online Installer.

---

## Step 1: Deep Clean (5 minutes)

**Run as Administrator:**
```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp
.\deep-clean-for-fresh-install.ps1
```

This removes:
- All Qt installations
- Chocolatey MinGW packages
- Build artifacts
- Qt-related PATH entries

**Then restart your computer** (recommended to clear file locks)

---

## Step 2: Download Qt Online Installer (2 minutes)

**Download Link:**
https://www.qt.io/download-qt-installer

**File:** `qt-unified-windows-x64-online.exe`

---

## Step 3: Install Qt with MinGW (20-30 minutes)

### Installation Steps:

1. **Run** `qt-unified-windows-x64-online.exe`

2. **Login/Create Qt Account** (free)

3. **Choose Installation Path:**
   - Default: `C:\Qt`
   - Or custom path (update scripts accordingly)

4. **Select Custom Installation**

5. **Select Components:**
   
   Under **Qt 5.15.2**:
   - ✅ **MinGW 8.1.0 64-bit** ← REQUIRED
   - ✅ Qt Charts (if needed)
   - ✅ Qt Data Visualization (if needed)
   - ✅ Sources (optional)
   
   Under **Developer and Designer Tools**:
   - ✅ **MinGW 8.1.0 64-bit** ← REQUIRED (compiler)
   - ✅ Qt Creator (optional but recommended)
   - ✅ CMake (optional - you already have it)
   - ✅ Ninja (optional)

6. **Accept Licenses**

7. **Install** (15-20 minutes download + install)

### What Gets Installed:

```
C:\Qt\
  ├── 5.15.2\
  │   └── mingw81_64\          ← Qt binaries for MinGW
  │       ├── bin\             (qmake, Qt DLLs)
  │       ├── lib\             (Qt libraries)
  │       └── include\         (Qt headers)
  └── Tools\
      └── mingw810_64\         ← MinGW compiler
          └── bin\             (g++, gcc, mingw32-make)
```

---

## Step 4: Verify Installation (1 minute)

```powershell
# Reload PATH
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine")

# Check MinGW
g++ --version
# Should show: g++ 8.1.0

# Check Qt
qmake --version
# Should show: Qt version 5.15.2

# Check paths
where.exe g++
# Should show: C:\Qt\Tools\mingw810_64\bin\g++.exe

where.exe qmake
# Should show: C:\Qt\5.15.2\mingw81_64\bin\qmake.exe
```

---

## Step 5: Configure PATH (if needed)

Qt installer usually adds paths automatically. If not:

```powershell
# Run as Administrator
[Environment]::SetEnvironmentVariable("Path", 
    $env:Path + ";C:\Qt\Tools\mingw810_64\bin;C:\Qt\5.15.2\mingw81_64\bin", "Machine")
```

---

## Step 6: Build Project (5-10 minutes)

```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp

# Configure
.\configure-project.bat

# Build
.\build-project.bat

# Run
.\run-app.bat
```

---

## What About vcpkg?

**Your existing vcpkg is PERFECT - keep it!**

You already have:
- ✅ Boost 1.90.0 for x64-mingw-dynamic
- ✅ OpenSSL 3.6.0 for x64-mingw-dynamic

These will work perfectly with Qt MinGW 8.1.0!

---

## Troubleshooting

### Issue: "g++ not found" after installation

**Solution:**
Manually add to PATH:
```powershell
[Environment]::SetEnvironmentVariable("Path", 
    $env:Path + ";C:\Qt\Tools\mingw810_64\bin", "Machine")
```

### Issue: CMake cannot find Qt

**Solution:**
Set CMAKE_PREFIX_PATH in configure-project.bat:
```batch
-DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\mingw81_64
```

### Issue: Build fails with "Qt5Core.lib not found"

**Cause:** Trying to use MSVC libraries instead of MinGW

**Solution:** Make sure CMAKE_PREFIX_PATH points to `mingw81_64` not `msvc2019_64`

---

## Why This Approach?

**Advantages:**
1. ✅ Single source (Qt Company)
2. ✅ Matching versions (Qt 5.15.2 + MinGW 8.1.0)
3. ✅ Tested together by Qt
4. ✅ Easy updates via Maintenance Tool
5. ✅ Official support

**What Changed from Before:**
- ❌ Before: Chocolatey MinGW 8.1.0 (broken, crashes)
- ✅ Now: Qt's bundled MinGW 8.1.0 (stable, tested)
- ❌ Before: No Qt MinGW binaries
- ✅ Now: Qt 5.15.2 compiled for MinGW

---

## Time Estimate

| Task | Time |
|------|------|
| Deep clean | 5 min |
| Download installer | 2 min |
| Qt installation | 20-30 min |
| Verification | 1 min |
| Build project | 5-10 min |
| **Total** | **35-50 min** |

---

## After Installation

### Daily Workflow:
```powershell
# Edit code
# Build
.\build-project.bat
# Run
.\run-app.bat
```

### Update Qt:
```powershell
C:\Qt\MaintenanceTool.exe
# Select "Add or remove components" or "Update components"
```

---

## Ready to Start?

1. **Run:** `.\deep-clean-for-fresh-install.ps1` (as Admin)
2. **Restart** your computer
3. **Download:** Qt Online Installer
4. **Install:** Qt 5.15.2 MinGW
5. **Build:** Your project

---

**Created:** 2025-01-09  
**Purpose:** Fresh Qt installation after deep clean  
**Next:** Run deep-clean script, then download Qt installer
