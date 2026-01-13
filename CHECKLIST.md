# Setup Completion Checklist

Use this checklist to track your progress through the setup process.

## Pre-Setup Verification

- [ ] PowerShell 7+ installed and working
- [ ] Administrator access available
- [ ] Internet connection active (for downloads)
- [ ] At least 5 GB free disk space
- [ ] Git repository on `refactored_dec` branch
- [ ] vcpkg already installed at `C:\vcpkg`

---

## Phase 1: Cleanup

### Cleanup Tasks
- [ ] Opened PowerShell as Administrator
- [ ] Navigated to project directory
- [ ] Ran `cleanup-environment.ps1` or `cleanup-environment.bat`
- [ ] Script completed without critical errors
- [ ] Confirmed `g++` not found in PATH (expected)
- [ ] Build directory removed
- [ ] Autogen directories removed

### Verification
```powershell
# These should NOT be found:
where.exe g++            # Should show: "INFO: Could not find..."
where.exe qmake          # Should show: "INFO: Could not find..." (or old installation)

# These folders should NOT exist:
Test-Path build                            # Should be False
Test-Path benchmark_qt_vs_native_autogen   # Should be False
```

**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 2: Install MinGW-w64

### Installation Tasks
- [ ] Ran command: `winget install --id BrechtSanders.WinLibs.POSIX.UCRT`
- [ ] Installation completed successfully
- [ ] MinGW installed to `C:\mingw64` (or noted alternate location: _________)

### Verification
```powershell
# Check installation
Test-Path C:\mingw64\bin\g++.exe          # Should be True
Test-Path C:\mingw64\bin\mingw32-make.exe # Should be True

# Check version (after adding to PATH)
g++ --version   # Should show: 13.2.0 or higher
```

**Installed Version:** __________  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 3: Install Qt 5.15.2

### Installation Tasks
- [ ] Downloaded Qt installer from: https://www.qt.io/download-qt-installer
- [ ] Created/logged into Qt account
- [ ] Ran installer
- [ ] Selected **Custom Installation**
- [ ] Selected components:
  - [ ] Qt 5.15.2 → MinGW 8.1.0 64-bit
  - [ ] Qt 5.15.2 → Sources (optional)
  - [ ] Developer Tools → Qt Creator (optional)
- [ ] Installation completed successfully
- [ ] Qt installed to `C:\Qt\5.15.2\mingw81_64` (or noted alternate: _________)

### Verification
```powershell
# Check installation
Test-Path C:\Qt\5.15.2\mingw81_64\bin\qmake.exe   # Should be True
Test-Path C:\Qt\5.15.2\mingw81_64\bin\Qt5Core.dll # Should be True

# Check version (after adding to PATH)
qmake --version   # Should show: Qt version 5.15.2
```

**Installed Version:** __________  
**Installation Path:** __________  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 4: Configure Environment

### Environment Variable Tasks
- [ ] Opened PowerShell as Administrator
- [ ] Added MinGW to PATH:
  ```powershell
  [Environment]::SetEnvironmentVariable("Path", 
    $env:Path + ";C:\mingw64\bin", "Machine")
  ```
- [ ] Added Qt to PATH:
  ```powershell
  [Environment]::SetEnvironmentVariable("Path", 
    $env:Path + ";C:\Qt\5.15.2\mingw81_64\bin", "Machine")
  ```
- [ ] Restarted PowerShell (or reloaded PATH)

### Verification
```powershell
# Verify PATH contains new entries
$env:Path -split ';' | Select-String -Pattern 'mingw64'
$env:Path -split ';' | Select-String -Pattern 'Qt'

# Verify tools accessible
where.exe g++      # Should show: C:\mingw64\bin\g++.exe
where.exe qmake    # Should show: C:\Qt\5.15.2\mingw81_64\bin\qmake.exe
where.exe cmake    # Should show cmake location

# Version checks
g++ --version      # Should show: 13.2.0+
qmake --version    # Should show: Qt 5.15.2
cmake --version    # Should show: 3.20+
```

**g++ Version:** __________  
**qmake Version:** __________  
**cmake Version:** __________  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 5: Configure Project

### Configuration Tasks
- [ ] Opened PowerShell in project directory
- [ ] Ran `.\configure-project.bat`
- [ ] CMake configuration completed successfully
- [ ] No critical errors in output
- [ ] `build/CMakeCache.txt` file created
- [ ] Boost libraries found
- [ ] OpenSSL libraries found
- [ ] Qt5 found

### Verification
```powershell
# Check build directory
Test-Path build\CMakeCache.txt              # Should be True
Test-Path build\Makefile                    # Should be True

# Check CMake configuration
Select-String -Path build\CMakeCache.txt -Pattern "Boost_FOUND"
Select-String -Path build\CMakeCache.txt -Pattern "Qt5_FOUND"
Select-String -Path build\CMakeCache.txt -Pattern "OPENSSL_FOUND"
```

**CMake Output:** ⬜ Success | ⬜ Warnings | ⬜ Errors  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 6: Build Project

### Build Tasks
- [ ] Ran `.\build-project.bat`
- [ ] Build completed successfully
- [ ] No compiler crashes
- [ ] `TradingTerminal.exe` created
- [ ] DLLs copied automatically (POST_BUILD commands)

### Verification
```powershell
# Check executable
Test-Path build\TradingTerminal.exe         # Should be True
(Get-Item build\TradingTerminal.exe).Length # Should be ~20-25 MB

# Check DLLs
Test-Path build\Qt5Core.dll                 # Should be True
Test-Path build\Qt5Gui.dll                  # Should be True
Test-Path build\Qt5Widgets.dll              # Should be True
Test-Path build\libboost*.dll               # Should find some
Test-Path build\libssl*.dll                 # Should be True
```

**Build Time:** __________ minutes  
**Executable Size:** __________ MB  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 7: Run Application

### Launch Tasks
- [ ] Ran `.\run-app.bat`
- [ ] Application window opened
- [ ] No missing DLL errors
- [ ] GUI renders correctly
- [ ] Console shows application logs

### Verification
- [ ] Application starts without errors
- [ ] Main window visible
- [ ] Menu bar accessible
- [ ] Can interact with UI elements
- [ ] Console shows initialization messages

**Launch Result:** ⬜ Success | ⬜ Errors | ⬜ DLL Missing  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Phase 8: Test BSE Parser (After Successful Build)

### BSE Parser Testing Tasks
- [ ] Application running with console visible
- [ ] Added token `1163264` to market watch
- [ ] Added token `1143697` to market watch
- [ ] Connected to BSE FO broadcast feed
- [ ] Receiving market data updates
- [ ] Console shows debug output for target tokens

### Debug Output Verification
Look for console output like:
```
[BSE 2020 DEBUG] Token: 1163264
  Known fields: token=1163264, ltp=1234.50, ...
  Unknown fields (48-83):
    field_48=XXXXX, field_52=XXXXX, ...
  Unknown fields (88-103):
    field_88=XXXXX, field_92=XXXXX, ...
```

### Data Captured
- [ ] Captured debug output for token 1163264
- [ ] Captured debug output for token 1143697
- [ ] Observed field value changes over time
- [ ] Identified potential field meanings:
  - [ ] noOfTrades (incrementing)
  - [ ] ltq (lot size multiples)
  - [ ] totalBuyQty (large values)
  - [ ] totalSellQty (large values)
  - [ ] lowerCircuit (near LTP ±%)
  - [ ] upperCircuit (near LTP ±%)

**Debug Output Captured:** ⬜ Yes | ⬜ No | ⬜ Partial  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Final Verification

### Overall System Check
- [ ] All phases completed successfully
- [ ] Development environment stable
- [ ] Can build project from scratch
- [ ] Application runs without errors
- [ ] BSE parser debug output visible
- [ ] Ready for production development

### Development Workflow Test
- [ ] Made a small code change (comment or log message)
- [ ] Ran `.\build-project.bat`
- [ ] Build completed quickly (incremental build)
- [ ] Ran `.\run-app.bat`
- [ ] Change reflected in application

**Incremental Build Time:** __________ seconds  
**Status:** ⬜ Not Started | ⬜ In Progress | ⬜ Completed

---

## Summary

### Total Time Spent
- Cleanup: __________ minutes
- MinGW Installation: __________ minutes
- Qt Installation: __________ minutes
- Environment Configuration: __________ minutes
- Project Build: __________ minutes
- Testing: __________ minutes
- **Total:** __________ minutes

### Issues Encountered
1. __________________________________________
2. __________________________________________
3. __________________________________________

### Issues Resolved
1. __________________________________________
2. __________________________________________
3. __________________________________________

### Outstanding Issues
1. __________________________________________
2. __________________________________________
3. __________________________________________

---

## Next Steps

After completing this checklist:

1. **Daily Development:**
   - Edit code
   - Run `.\build-project.bat`
   - Run `.\run-app.bat`
   - Test changes

2. **BSE Parser Analysis:**
   - Review captured debug output
   - Identify field patterns
   - Update parser code with correct field mappings
   - Document findings

3. **Git Workflow:**
   - Commit working changes
   - Push to `refactored_dec` branch
   - Create feature branches as needed

4. **Documentation:**
   - Update field mappings in code comments
   - Document any BSE protocol findings
   - Update README if needed

---

## Support Resources

- **SETUP_GUIDE.md** - Comprehensive setup instructions
- **QUICK_START.md** - Quick reference guide
- **SUMMARY.md** - Overview of all files
- **START_HERE.txt** - Visual quick start guide

---

**Checklist Created:** 2025-01-02  
**Last Updated:** __________  
**Completed By:** __________  
**Completion Date:** __________

---

## Notes

Use this space for any additional notes during setup:

_________________________________________________________________

_________________________________________________________________

_________________________________________________________________

_________________________________________________________________

_________________________________________________________________
