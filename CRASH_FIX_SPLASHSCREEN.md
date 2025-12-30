# Crash Fix: SplashScreen Double-Free
## Issue: SIGABRT - Pointer Being Freed Was Not Allocated

---

## 🔴 Problem

The application was crashing with:
```
___BUG_IN_CLIENT_OF_LIBMALLOC_POINTER_BEING_FREED_WAS_NOT_ALLOCATED
```

**Crash Location**: `main_work.cpp:133`
```cpp
delete splash;  // ❌ CRASH - Double free!
```

---

## 🔍 Root Cause

**Qt Widget Lifecycle Issue**

When you `delete` a Qt widget (like `SplashScreen`) that's still in the Qt event loop:
1. The widget is immediately freed
2. Qt's event loop still has pending events for this widget
3. When Qt tries to process those events, it accesses freed memory
4. This causes a double-free or use-after-free crash

**Why it happened**:
- `SplashScreen` was shown with `show()` (non-modal)
- It was still in the event loop when `delete` was called
- Qt's internal cleanup tried to delete it again → crash

---

## ✅ Solution

**Use `deleteLater()` instead of `delete`**

```cpp
// ❌ WRONG - Immediate deletion causes crash
delete splash;

// ✅ CORRECT - Safe deletion after event loop processes pending events
splash->deleteLater();
```

**What `deleteLater()` does**:
1. Marks the widget for deletion
2. Waits for all pending events to be processed
3. Deletes the widget safely when the event loop is idle
4. Prevents double-free and use-after-free bugs

---

## 📝 Fixed Code

**Before** (Crashed):
```cpp
QTimer::singleShot(500, [splash]() {
    splash->close();
    delete splash;  // ❌ CRASH
    // ...
});
```

**After** (Fixed):
```cpp
QTimer::singleShot(500, [splash]() {
    splash->close();
    splash->deleteLater();  // ✅ Safe
    // ...
});
```

---

## 🎓 Qt Best Practices

### When to use `delete`:
- ✅ Objects you created with `new` that are NOT Qt widgets
- ✅ Objects that are NOT in the event loop
- ✅ Objects in destructors (if you own them)

### When to use `deleteLater()`:
- ✅ **ALL Qt widgets** (QWidget, QDialog, QMainWindow, etc.)
- ✅ **ALL QObject-derived classes** in event loops
- ✅ Objects that might have pending signals/slots
- ✅ Objects that might have pending events

### Example:
```cpp
// ✅ CORRECT
QWidget *widget = new QWidget();
widget->show();
// ... later ...
widget->deleteLater();  // Safe

// ✅ CORRECT
QString *str = new QString("hello");
delete str;  // OK - QString is not a QObject

// ❌ WRONG
QWidget *widget = new QWidget();
widget->show();
delete widget;  // CRASH - widget is in event loop!
```

---

## 🔧 Testing

After the fix:
```bash
cmake --build build
cd build
./TradingTerminal.app/Contents/MacOS/TradingTerminal
```

**Expected behavior**:
- ✅ Splash screen shows
- ✅ Progress bar animates
- ✅ Splash closes smoothly
- ✅ Main window appears
- ✅ No crash!

---

## 📚 Related Qt Documentation

- [QObject::deleteLater()](https://doc.qt.io/qt-5/qobject.html#deleteLater)
- [Object Trees & Ownership](https://doc.qt.io/qt-5/objecttrees.html)
- [Memory Management in Qt](https://doc.qt.io/qt-5/memory.html)

---

## 💡 Key Takeaway

**Golden Rule**: In Qt, **ALWAYS use `deleteLater()` for widgets and QObjects**, never `delete`.

This prevents:
- Double-free crashes
- Use-after-free bugs
- Segmentation faults
- Memory corruption

---

*Fixed: 2025-12-30 02:01 IST*
