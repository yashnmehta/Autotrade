# Qt Focus Policies Explained

## What is Qt::StrongFocus?

**Qt::StrongFocus** is a **focus policy** that determines **HOW** a widget can receive keyboard focus, not **WHETHER** it can escape focus.

---

## All Qt Focus Policies

| Policy | Tab Key | Click | Mouse Wheel | Use Case |
|--------|---------|-------|-------------|----------|
| `Qt::NoFocus` | ❌ | ❌ | ❌ | Read-only displays, labels |
| `Qt::TabFocus` | ✅ | ❌ | ❌ | Widgets that only need keyboard navigation |
| `Qt::ClickFocus` | ❌ | ✅ | ❌ | Widgets that only need mouse interaction |
| `Qt::StrongFocus` | ✅ | ✅ | ✅ | **Most interactive widgets** (combo boxes, buttons) |
| `Qt::WheelFocus` | ✅ | ✅ | ✅ | Same as StrongFocus + wheel events (rare) |

---

## Qt::StrongFocus Breakdown

```cpp
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);
```

**What this means:**
1. ✅ **Tab key** can give this widget focus
2. ✅ **Mouse click** can give this widget focus
3. ✅ **Mouse wheel** scroll can give this widget focus (useful for combo boxes)
4. ✅ Widget accepts keyboard input when focused
5. ❌ **DOES NOT** prevent focus from escaping to other widgets

---

## Common Misconception

### ❌ Wrong Understanding
> "StrongFocus means focus is 'strongly trapped' within the widget"

### ✅ Correct Understanding
> "StrongFocus means the widget 'strongly accepts' focus from multiple input methods (Tab, Click, Wheel)"

---

## The Two Separate Concepts

### 1. Focus Policy (HOW to receive focus)

```cpp
// Focus Policy: HOW can this widget receive focus?
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);  // Tab + Click + Wheel
m_segmentCombo->setFocusPolicy(Qt::TabFocus);      // Tab only
m_tokenEdit->setFocusPolicy(Qt::NoFocus);          // Never receives focus
```

**Analogy:** Focus policy is like a door's lock type
- `NoFocus` = No door (can't enter)
- `TabFocus` = Keycard only (Tab key)
- `ClickFocus` = Touch sensor only (Mouse click)
- `StrongFocus` = Keycard + Touch + Wheel (Multiple ways in)

---

### 2. Focus Trapping (PREVENTING focus from escaping)

```cpp
// Focus Trapping: WHERE can focus go from here?
bool ScripBar::focusNextPrevChild(bool next) {
  // Build internal focus chain
  // Calculate next widget within chain
  // Set focus to that widget
  return true;  // ← This prevents escape!
}
```

**Analogy:** Focus trapping is like a fence around buildings
- Without trapping: Focus can walk between any buildings
- With trapping: Focus can only walk between buildings inside the fence

---

## Real-World Example

### Scenario: ScripBar with StrongFocus but NO Trapping

```cpp
// Setup
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);
m_segmentCombo->setFocusPolicy(Qt::StrongFocus);
m_addToWatchButton->setFocusPolicy(Qt::StrongFocus);
// NO focusNextPrevChild() override
```

**User interactions:**
| Action | Result |
|--------|--------|
| Click on Exchange combo | ✅ Gets focus (StrongFocus allows click) |
| Tab from Exchange | ✅ Moves to Segment (StrongFocus allows Tab) |
| Tab from Add button | ❌ **Focus escapes to MarketWatch** (no trapping) |
| Scroll wheel on Symbol combo | ✅ Gets focus (StrongFocus allows wheel) |

---

### Scenario: ScripBar with StrongFocus AND Trapping

```cpp
// Setup
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);
m_segmentCombo->setFocusPolicy(Qt::StrongFocus);
m_addToWatchButton->setFocusPolicy(Qt::StrongFocus);

// Focus trapping override
bool ScripBar::focusNextPrevChild(bool next) {
  // ... internal cycling logic ...
  return true;  // ← Prevents escape
}
```

**User interactions:**
| Action | Result |
|--------|--------|
| Click on Exchange combo | ✅ Gets focus (StrongFocus allows click) |
| Tab from Exchange | ✅ Moves to Segment (StrongFocus allows Tab) |
| Tab from Add button | ✅ **Wraps to Exchange** (trapping prevents escape) |
| Scroll wheel on Symbol combo | ✅ Gets focus (StrongFocus allows wheel) |

---

## Why We Need Both

### StrongFocus (Focus Policy)

**Purpose:** Allow multiple ways to focus the widget

```cpp
m_symbolCombo->setFocusPolicy(Qt::StrongFocus);
```

**Enables:**
- ✅ User can Tab to this widget
- ✅ User can click on this widget
- ✅ User can scroll wheel on this widget
- ✅ Widget appears in the focus chain

**Without StrongFocus:**
```cpp
m_symbolCombo->setFocusPolicy(Qt::NoFocus);  // ← Widget excluded from Tab order
```
- ❌ Tab key skips this widget entirely
- ❌ Can't be focused at all (even by mouse)
- ❌ Not in focus chain (even with trapping)

---

### Focus Trapping (focusNextPrevChild Override)

**Purpose:** Control WHERE focus goes when Tab is pressed

```cpp
bool ScripBar::focusNextPrevChild(bool next) {
  // Cycle within ScripBar only
  return true;  // Consumed
}
```

**Enables:**
- ✅ Tab cycles within ScripBar
- ✅ Prevents escape to other widgets
- ✅ Implements custom wrapping logic

**Without Focus Trapping:**
```cpp
// Use Qt's default focusNextPrevChild()
// (inherited from QWidget)
```
- ❌ Tab follows global focus chain
- ❌ Focus escapes ScripBar
- ❌ No custom wrapping

---

## Comparison Table

| Feature | Qt::StrongFocus | Focus Trapping |
|---------|-----------------|----------------|
| **What it controls** | How to **receive** focus | Where focus **goes next** |
| **Scope** | Individual widget | Container (ScripBar) |
| **Set via** | `setFocusPolicy()` | Override `focusNextPrevChild()` |
| **Affects Tab behavior** | Makes widget **accessible** via Tab | Makes focus **stay within** container |
| **Affects Click behavior** | Makes widget **focusable** via click | No effect on click |
| **Can be combined** | ✅ Yes - **we use both!** | |

---

## What If We Only Had One?

### Only StrongFocus, No Trapping ❌

```cpp
// All widgets have StrongFocus
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);
m_segmentCombo->setFocusPolicy(Qt::StrongFocus);
// ... etc

// NO focusNextPrevChild() override
```

**Result:**
- ✅ Can Tab between ScripBar widgets
- ✅ Can click on ScripBar widgets
- ❌ **Tab escapes ScripBar to other windows**
- ❌ No wrapping behavior

---

### Only Trapping, No StrongFocus ❌

```cpp
// Widgets have default/weak focus policy
m_exchangeCombo->setFocusPolicy(Qt::NoFocus);  // ← Oops!
m_segmentCombo->setFocusPolicy(Qt::NoFocus);
// ... etc

// HAS focusNextPrevChild() override
bool ScripBar::focusNextPrevChild(bool next) {
  // Try to cycle within ScripBar
  return true;
}
```

**Result:**
- ❌ **Widgets excluded from focus chain entirely**
- ❌ Tab key does nothing (no widgets to focus)
- ❌ Click doesn't work to focus widgets
- ✅ Focus trapped (but irrelevant - nothing to trap!)

---

### Both StrongFocus AND Trapping ✅

```cpp
// All widgets have StrongFocus
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);
m_segmentCombo->setFocusPolicy(Qt::StrongFocus);
// ... etc

// HAS focusNextPrevChild() override
bool ScripBar::focusNextPrevChild(bool next) {
  // Cycle within visible StrongFocus widgets
  return true;
}
```

**Result:**
- ✅ Can Tab between ScripBar widgets
- ✅ Can click on ScripBar widgets
- ✅ **Tab stays within ScripBar (trapped)**
- ✅ Wrapping behavior works
- ✅ **Perfect combination!**

---

## Visual Analogy

### Focus Policy = Building Entrances

```
┌─────────────────────┐
│   Exchange Combo    │  ← Qt::StrongFocus
│                     │
│  🚪 Tab entrance    │  ✅ Can enter via Tab
│  🖱️  Click entrance  │  ✅ Can enter via Click
│  🖲️  Wheel entrance  │  ✅ Can enter via Wheel
└─────────────────────┘

┌─────────────────────┐
│   Token Edit        │  ← Qt::NoFocus
│                     │
│  ⛔ No entrances    │  ❌ Can't enter at all
└─────────────────────┘
```

---

### Focus Trapping = Fence Around Buildings

```
Without Trapping:
┌──────────┐     ┌──────────┐     ┌──────────┐
│ Exchange │ Tab │ Segment  │ Tab │MarketWatch│ ← Escaped!
│  Combo   │ ───>│  Combo   │ ───>│  Table   │
└──────────┘     └──────────┘     └──────────┘

With Trapping:
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃         ScripBar Fence             ┃
┃  ┌──────────┐     ┌──────────┐    ┃
┃  │ Exchange │ Tab │ Segment  │    ┃
┃  │  Combo   │ ───>│  Combo   │    ┃
┃  └──────────┘     └──────────┘    ┃
┃        ↑                ↓          ┃
┃        │    ┌──────────┐          ┃
┃        └───│   Add     │          ┃
┃        Tab │  Button   │          ┃  ← Trapped!
┃            └──────────┘           ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
```

---

## Code Walkthrough

### Step 1: Set Focus Policies (Individual Widget Level)

```cpp
void ScripBar::setupUI() {
  // ...create widgets...
  
  // 🚪 Enable multiple focus methods for each widget
  m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);    // Tab + Click + Wheel
  m_segmentCombo->setFocusPolicy(Qt::StrongFocus);     // Tab + Click + Wheel
  m_symbolCombo->setFocusPolicy(Qt::StrongFocus);      // Tab + Click + Wheel
  m_addToWatchButton->setFocusPolicy(Qt::StrongFocus); // Tab + Click
  m_tokenEdit->setFocusPolicy(Qt::NoFocus);            // Never focusable
  
  // Each widget now has "multiple entrances"
}
```

---

### Step 2: Set Up Tab Order (Static Hints)

```cpp
void ScripBar::setupTabOrder() {
  // 🗺️ Give Qt hints about preferred tab sequence
  setTabOrder(m_exchangeCombo, m_segmentCombo);
  setTabOrder(m_segmentCombo, m_instrumentCombo);
  // ... etc
  
  // Qt will use this as a guideline
  // But our focusNextPrevChild() override takes precedence
}
```

---

### Step 3: Implement Focus Trapping (Container Level)

```cpp
bool ScripBar::focusNextPrevChild(bool next) {
  // 🏗️ Build focus chain from widgets with StrongFocus
  QList<QWidget*> focusChain;
  for (auto* widget : allWidgets) {
    if (widget->focusPolicy() != Qt::NoFocus) {  // ← Checks focus policy!
      focusChain.append(widget);
    }
  }
  
  // 🔄 Cycle within this chain only
  int nextIdx = calculateWrappedIndex(...);
  focusChain[nextIdx]->setFocus();
  
  // 🔒 Prevent escape
  return true;  // Tab event consumed
}
```

**Notice:** Focus trapping **depends on** focus policies!
- Widgets with `Qt::NoFocus` → Excluded from chain
- Widgets with `Qt::StrongFocus` → Included in chain

---

## Summary

### Qt::StrongFocus
**"This widget accepts focus from Tab, Click, and Wheel"**
- ✅ Makes widget **participating** in focus system
- ✅ Allows multiple input methods
- ❌ Does NOT prevent focus from escaping

### Focus Trapping (focusNextPrevChild)
**"When Tab is pressed, cycle focus within this container"**
- ✅ Prevents focus from escaping
- ✅ Implements custom wrapping logic
- ❌ Does NOT control how widgets receive focus initially

### Together
**"Widgets accept focus easily (StrongFocus) but don't let it escape (Trapping)"**
- ✅ Best user experience
- ✅ Professional keyboard navigation
- ✅ Predictable behavior

---

## Practical Test

Try this experiment:

```cpp
// Experiment 1: Remove StrongFocus, keep trapping
m_exchangeCombo->setFocusPolicy(Qt::NoFocus);  // ← Changed
// Result: Can't tab to widget, trapping is irrelevant

// Experiment 2: Remove trapping, keep StrongFocus  
// Comment out: bool ScripBar::focusNextPrevChild(...)
// Result: Can tab to widget, but focus escapes ScripBar

// Experiment 3: Both (our implementation)
m_exchangeCombo->setFocusPolicy(Qt::StrongFocus);  // ← Multiple entrances
bool ScripBar::focusNextPrevChild(...) { return true; }  // ← Trapped
// Result: Perfect! ✅
```

---

## Analogy Time!

**Focus Policy = Job Interview Eligibility**
- `NoFocus` = Not eligible to apply (no door)
- `TabFocus` = Can apply via online form only
- `ClickFocus` = Can apply via walk-in only
- `StrongFocus` = Can apply via online, walk-in, or referral (multiple ways)

**Focus Trapping = Company Rotation Policy**
- Without trapping = After your project, you might get assigned to different company
- With trapping = You always rotate within the same team

**Both = Internal rotation program for employees who can join via multiple channels**

---

Hope this clears up the confusion! **StrongFocus** and **Focus Trapping** are complementary features that work together to create the perfect keyboard navigation UX. 🎯
