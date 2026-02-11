# 🚀 MSVC Quick Reference

## ONE-TIME Setup (1-2 minutes)
```cmd
setup.bat
```
Configures CMake with Ninja. **Never needs to run again!**

---

## Daily Development Workflow

### Option 1: Standard (Recommended)
```cmd
build.bat    # 10-30 sec for changes, 5-8 min first time
run.bat      # Launch app
```

### Option 2: Super Fast
```cmd
init_msvc_env.bat    # Once per terminal session
quick_build.bat      # 5-15 sec - NO overhead!
run.bat              # Launch app
```

---

## Scripts Explained

| Script | When to Use | Time | Notes |
|--------|------------|------|-------|
| **setup.bat** | Once (first time only) | 1-2 min | Auto-detects if already configured |
| **build.bat** | After code changes | 10-30 sec⚡ | Handles everything automatically |
| **quick_build.bat** | Rapid iteration | 5-15 sec⚡⚡ | Requires MSVC env in terminal |
| **run.bat** | Launch app | Instant | Auto-deploys DLLs |
| **clean.bat** | Force rebuild | 5 sec | Keeps configuration |
| **init_msvc_env.bat** | Manual env setup | 5 sec | Usually not needed |

---

## Why So Fast?

✅ **Ninja** (not MSBuild) - 3-5x faster  
✅ **Incremental builds** - only changed files  
✅ **Multi-core** - uses all CPU cores  
✅ **Smart caching** - CMake doesn't reconfigure unnecessarily  

---

## Common Questions

**Q: Do I run setup.bat every time?**  
❌ NO! Only once, or when CMakeLists.txt changes.

**Q: Why was it taking 10 minutes before?**  
Because scripts were reconfiguring CMake every time. Fixed now!

**Q: What about first build?**  
First build: 5-8 minutes (compiling everything)  
After that: 10-30 seconds (only changes)

**Q: Can I use Visual Studio IDE?**  
Yes! Open `build_msvc\TradingTerminal.sln` but command-line Ninja is faster.

---

## Your VS 2026 Setup

✅ Visual Studio 18 (2026) detected  
✅ Ninja included: `C:\Program Files\Microsoft Visual Studio\18\...\ninja.exe`  
✅ Qt 5.15.2 MSVC configured  
✅ Auto-DLL deployment enabled  

**You're all set! Just run `build.bat`**
