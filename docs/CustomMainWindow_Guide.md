# CustomMainWindow - Reusable Frameless Window Component

## Overview
`CustomMainWindow` is a complete, production-ready replacement for `QMainWindow` when you need a frameless window with custom styling. It implements **all** the functionality that native window frames provide.

## Features Implemented ✅

### 1. Custom Title Bar
- Draggable title bar (drag anywhere to move window)
- Minimize/Maximize/Close buttons
- Custom title display
- Menu bar integration support

### 2. 8-Direction Window Resizing
Resize from any edge or corner:
- **Edges**: Top, Bottom, Left, Right
- **Corners**: TopLeft, TopRight, BottomLeft, BottomRight
- Automatic cursor changes (resize arrows)
- Respects minimum/maximum size constraints

### 3. Window State Management
- **Normal → Maximized**: Click maximize or double-click title bar
- **Maximized → Normal**: Restores to previous size and position
- **Minimize**: Minimizes to taskbar
- Geometry persistence across state changes

### 4. Smart Maximize
- Detects screen boundaries (multi-monitor aware)
- Excludes taskbar area (`availableGeometry`)
- Smooth transitions

### 5. Constraints
- Configurable minimum size (default: 400×300)
- Configurable maximum size (default: unlimited)
- Enforced during manual resizing

## Usage

### Basic Usage
```cpp
#include "ui/CustomMainWindow.h"

CustomMainWindow *window = new CustomMainWindow();
window->setTitle("My Trading App");
window->resize(1280, 720);
window->show();
```

### With Central Widget
```cpp
CustomMainWindow *window = new CustomMainWindow();
window->setTitle("Market Data");

// Your content widget
QWidget *content = new QWidget();
QVBoxLayout *layout = new QVBoxLayout(content);
// ... add your UI here

window->setCentralWidget(content);
window->show();
```

### Set Constraints
```cpp
window->setMinimumSize(800, 600);
window->setMaximumSize(1920, 1080);
```

### Inherit for Application-Specific Windows
```cpp
class MainWindow : public CustomMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr)
        : CustomMainWindow(parent)
    {
        setTitle("Trading Terminal");
        setupMyContent();
    }
    
private:
    void setupMyContent() {
        // Create your MDI area, toolbars, etc.
        QWidget *container = new QWidget();
        // ... setup
        setCentralWidget(container);
    }
};
```

## Architecture

### Inheritance Hierarchy
```
QWidget
  └── CustomMainWindow (reusable frameless base)
        └── MainWindow (trading terminal specific)
```

### Key Design Decisions

1. **QWidget, not QMainWindow**: QMainWindow adds complexity we don't need
2. **Manual Event Handling**: Full control over mouse events for resize/drag
3. **State Machine**: Proper geometry management for maximize/restore
4. **No Platform-Specific Code**: Pure Qt, works on Windows/macOS/Linux

## Cross-Platform Compatibility

| Feature | Windows | macOS | Linux |
|---------|---------|-------|-------|
| Frameless Window | ✅ | ✅ | ✅ |
| Resize (8 directions) | ✅ | ✅ | ✅ |
| Maximize to available screen | ✅ | ✅ | ✅ |
| Multi-monitor support | ✅ | ✅ | ✅ |
| Taskbar minimize | ✅ | ✅ | ✅ |

## Customization Points

### Add Custom Buttons to Title Bar
```cpp
CustomMainWindow *window = new CustomMainWindow();
QPushButton *settingsBtn = new QPushButton("⚙");
window->titleBar()->layout()->addWidget(settingsBtn);
```

### Override Window Styling
```cpp
window->setStyleSheet(
    "CustomMainWindow { "
    "   background-color: #1e1e1e; "
    "   border: 2px solid #007acc; "  // Blue accent border
    "}"
);
```

### Connect to State Changes
```cpp
connect(window, &CustomMainWindow::windowStateChanged, 
    [](bool isMaximized) {
        qDebug() << "Window is" << (isMaximized ? "maximized" : "normal");
    }
);
```

## Performance

- **Zero overhead**: Direct event handling, no wrapper layers
- **Native resize**: Uses Qt's geometry APIs efficiently
- **Optimized cursor updates**: Only recalculates on hover/move

## Future Enhancements (Optional)

- ⬜ Window snapping (Aero Snap style)
- ⬜ Drop shadow effect (platform-specific)
- ⬜ Rounded corners (modern UI trend)
- ⬜ Custom animations for maximize/minimize
- ⬜ Save/restore geometry to settings

## Comparison: QMainWindow vs CustomMainWindow

| Feature | QMainWindow | CustomMainWindow |
|---------|-------------|------------------|
| Native frame | ✅ (OS-controlled) | ❌ (Custom) |
| Custom title bar | ❌ | ✅ |
| Full styling control | ❌ | ✅ |
| Resize customization | ❌ | ✅ |
| Cross-platform look | ❌ (OS-dependent) | ✅ (Consistent) |
| Performance | Good | **Excellent** |
