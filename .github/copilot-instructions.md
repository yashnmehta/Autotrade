# GitHub Copilot Instructions for Trading Terminal C++

## Build System

### IMPORTANT: MSVC Build Only

This project **MUST** be built with MSVC compiler in the `build_ninja` directory.

**Why MSVC?**
- MinGW compiled Qt library does **NOT** support QtWebEngine
- QtWebEngine (Chromium) requires MSVC on Windows
- TradingView chart integration depends on QtWebEngine

### Build Command

```bash
C:\Users\admin\Desktop\trading_terminal_cpp\build_ninja\build.bat
```

**Build Script Details:**
- Uses Ninja build system for fast incremental builds
- Automatically calls vcvars64.bat to setup MSVC environment
- Typical build time: <1 second for single file changes
- Full rebuild: 10-30 seconds

### Build Directory Structure

```
build_ninja/          # MSVC + Ninja (USE THIS)
├── build.bat         # Fast build script
├── build.ninja       # Ninja build file
├── TradingTerminal.exe
└── run.bat

build/                # Legacy directory (DO NOT USE)
build_msvc/           # MSVC solution files (DO NOT USE)
```

### Running the Application

```bash
cd build_ninja
.\TradingTerminal.exe
```

## Code Changes Workflow

1. Edit source files in `src/` or `include/`
2. Run: `cd build_ninja && .\build.bat`
3. If successful, run: `.\TradingTerminal.exe`
4. Monitor console output for errors

## Common Issues

- **Error: QtWebEngine not found** → You're using MinGW Qt. Must use MSVC Qt.
- **Linker errors** → Clean rebuild: `ninja clean && ninja`
- **Missing DLLs** → Ensure Qt MSVC binaries are in PATH

## Project Architecture

- **Qt Version**: 5.15.2 (MSVC 2019 64-bit)
- **Build System**: CMake + Ninja
- **Compiler**: MSVC 2019/2022
- **Key Dependencies**: QtWebEngine, QtWebChannel, QtNetwork, QtWidgets

---

## UI Architecture — .ui File Approach

### IMPORTANT: Qt Designer `.ui` Files are the Single Source of Truth

This project uses **Qt Designer `.ui` files** for all dialog and window layouts.
**Do NOT create UI widgets programmatically in `.cpp` files** for dialogs/windows
that already have a `.ui` file.

### Where `.ui` Files Live

```
resources/forms/
├── StrategyTemplateBuilderDialog.ui   ← Strategy template builder
├── StrategyManagerWindow.ui           ← Strategy manager panel
├── CreateStrategyDialog.ui            ← Legacy create dialog
└── ...
```

### The Pattern

```cpp
// .h file
#include "ui_MyDialog.h"
class MyDialog : public QDialog {
    Ui::MyDialog *ui;
};

// .cpp file
MyDialog::MyDialog(QWidget *parent) : QDialog(parent), ui(new Ui::MyDialog) {
    ui->setupUi(this);
    // Wire signals/slots only — NO setStyleSheet() or widget creation here
    connect(ui->myButton, &QPushButton::clicked, this, &MyDialog::onClicked);
}
```

### Stylesheet Rules

- **Stylesheets belong in the `.ui` file** — the `<property name="styleSheet">` block
  on the root widget is the **single source of truth** for the entire dialog's theme.
- **Never add a duplicate `setStyleSheet()` call** in the `.cpp` constructor for
  dialogs that have a `.ui` file — it will conflict and the last one applied wins,
  causing hard-to-debug visual glitches (e.g. dark/light mix).
- For dialogs built entirely in C++ (no `.ui` file), the stylesheet goes in the
  constructor as normal.

### Light Theme Palette (use these everywhere)

| Token            | Hex                   | Usage                        |
|------------------|-----------------------|------------------------------|
| Background       | `#ffffff`             | Dialog / widget base         |
| Surface          | `#f8fafc`             | GroupBox, table header       |
| Border           | `#e2e8f0`             | All borders / dividers       |
| Border focus     | `#3b82f6`             | Input focus ring             |
| Text primary     | `#1e293b`             | Main readable text           |
| Text secondary   | `#475569`             | Labels, hints                |
| Text muted       | `#64748b`             | Placeholders, meta info      |
| Input bg         | `#ffffff`             | QLineEdit, QComboBox, etc.   |
| Selection bg     | `#dbeafe`             | Table / list selection       |
| Selection text   | `#1e40af`             | Table / list selected text   |
| Accent blue      | `#2563eb`             | Primary action buttons       |
| Accent green     | `#16a34a`             | Add / confirm buttons        |
| Accent red       | `#dc2626`             | Delete / remove buttons      |
| Title bar        | `#1e40af → #2563eb`   | Dialog title bar gradient    |

### Adding a New Dialog

1. Create the layout in Qt Designer → save to `resources/forms/MyDialog.ui`
2. Add the full light-theme stylesheet (see palette above) in the root widget's
   `styleSheet` property inside the `.ui` file
3. In `CMakeLists.txt`, add the `.ui` to the `qt5_wrap_ui()` / `ui_files` list
4. In the `.cpp`, call `ui->setupUi(this)` and wire signals — nothing else
5. For dynamic widgets created at runtime (e.g. `IndicatorRowWidget` cards added
   on button press), set their stylesheet in their own class constructor
