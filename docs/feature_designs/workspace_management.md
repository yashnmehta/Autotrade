# Workspace Management System - Design & Roadmap

## 1. Overview
The Workspace Management System allows users to save, manage, and restore their window layouts and configurations. A "Workspace" is a snapshot of the application's current state, enabling traders to switch contexts instantly (e.g., from an "F&O Analysis" layout to a "Morning Execution" layout).

### distinction: Workspace vs. Global Preferences
It is critical to distinguish between what is stored in a Workspace and what remains in Global Preferences.

| Feature Category | Global Preferences (`QSettings`) | Workspace (`QSettings` Group) |
| :--- | :--- | :--- |
| **Scope** | Application-wide, persistent across all sessions. | Specific to a named layout. |
| **Examples** | Theme (Dark/Light), Default Order Quantity, API Keys, Broadcast IPs. | Window Positions, Window Sizes, Open Window Types. |
| **Content State** | N/A | Specific scrips loaded in MarketWatch, Specific chart symbol open. |
| **Storage Key** | `General/Theme`, `Order/DefaultQty` | `workspaces/{name}/window_1/...` |

---

## 2. Technical Architecture

### 2.1 Current State Analysis
*   **Storage Mechanism**: `QSettings` is used correctly.
*   **Save Functionality**: Partially implemented in `CustomMDIArea::saveWorkspace`. It saves geometry and window type.
*   **Load Functionality**: **Broken**. `CustomMDIArea` reads the config but cannot instantiate windows because it lacks knowledge of the specific classes (Dependency Inversion principle violation).

### 2.2 Proposed Architecture
To fix the load functionality and support rich state restoration (e.g., loading the correct portfolio into a restored Market Watch), we will implement a **Factory Pattern** with **State Restoration**.

#### A. The Factory Protocol
`CustomMDIArea` should not instantiate windows. Instead, it should request the `MainWindow` (the controller) to create them.

1.  **`CustomMDIArea`**: When loading, iterates through keys and emits a signal for each window:
    ```cpp
    signals: void restoreWindowRequested(const QString& type, const QVariantMap& properties);
    ```
2.  **`MainWindow`**: Connects to this signal. It uses a factory method to instantiate the correct widget (`MarketWatchWindow`, `OrderBookWindow`, etc.).

#### B. The Restoration Interface
Every restoreable window must implement a restoration interface to load its internal state (beyond just geometry).

```cpp
// Interface or Base Class Method in CustomMDISubWindow
virtual void saveState(QSettings& settings) = 0;
virtual void restoreState(QSettings& settings) = 0;
```

**Example: MarketWatchWindow Restoration**
*   **Save**: Saves the list of tokens or a file path to the loaded portfolio.
*   **Restore**: Reads the tokens/file path and re-initializes the table model.

---

## 3. Data Structure
The workspace will be stored in `QSettings` under the group `workspaces/{workspace_name}`.

```ini
[workspaces/MorningTrade]
windowCount=2

# Window 0: Market Watch
window_0/type="MarketWatch"
window_0/geometry=@Rect(0 0 400 600)
window_0/title="Nifty Watch"
window_0/content/portfolio_file="/users/docs/nifty_morning.json"  <-- Deep State

# Window 1: Chart
window_1/type="Chart"
window_1/geometry=@Rect(400 0 800 600)
window_1/content/symbol="NIFTY"
window_1/content/timeframe="5min"
```

---

## 4. Implementation Roadmap

### Phase 1: Window Factory & Basic Reconstruction (Foundation)
**Goal**: Identify window types and restore empty windows in correct positions.
1.  **Define Signal**: Add `restoreWindowRequested` signal to `CustomMDIArea`.
2.  **Implement Factory**: Create `MainWindow::createWindowByType(QString type)` helper.
3.  **Wire Logic**: Connect `CustomMDIArea`'s load loop to the `MainWindow` factory.
4.  **Geometry Restore**: Ensure restored windows respect the saved `geometry`, `minimized`, and `maximized` states.

### Phase 2: Deep State Persistence (Content Restoration)
**Goal**: Restore the *contents* of the windows (e.g., scrips in MarketWatch).
1.  **Base Class Update**: Add `saveState(QSettings&)` and `restoreState(QSettings&)` to `CustomMDISubWindow`.
2.  **MarketWatch Implementation**:
    *   Implement `saveState`: Serialize current scrip list (or helper reference).
    *   Implement `restoreState`: Re-add scrips.
3.  **Update MDIArea**: Modify `saveWorkspace` to call `window->saveState()` and `loadWorkspace` to call `window->restoreState()` after creation.

### Phase 3: Workspace Management UI
**Goal**: Allow users to create, delete, and switch workspaces easily.
1.  **Menu Integration**: Ensure "Save Workspace" and "Load Workspace" menu items in `MainWindow` function correctly with input dialogs.
2.  **Manager Dialog**: Create a "Manage Workspaces" dialog to rename/delete existing workspaces.
3.  **Auto-Save**: Option to auto-save the "Current" workspace on exit.

---

## 5. Next Steps
1.  **Immediate**: Fix the broken `loadWorkspace` loop in `CustomMDIArea.cpp` to emit a signal instead of doing nothing.
2.  **Immediate**: Implement the `onRestoreWindowRequested` slot in `MainWindow`.
