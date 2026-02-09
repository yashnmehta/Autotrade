# Strategy Manager V2 - UI Spec

Date: 2026-02-09
Goal: A trader-first, dense, fast UI that scales to 100+ running instances.

---

## Layout Overview

1) Top Filter Bar
- Strategy Type (dropdown)
- Status (segmented buttons)
- Segment (dropdown)
- Account (dropdown)
- Search box (instance name, symbol)

2) Action Bar
- Create | Start | Pause | Resume | Modify | Stop | Delete
- Quick Actions: Kill All, Pause All, Resume All

3) Main Table (high density)
- Columns: Instance | Status | MTM | SL | Target | Entry | Qty | Pos | Ord | Dur | Symbol | Type | Updated
- Sorting enabled
- Row selection

4) Right Context Panel (optional, collapsible)
- Instance Summary
- Parameters (read-only)
- Last Error
- Recent Actions (audit)

5) Bottom System Strip
- Feed status
- Execution latency
- Order success rate

---

## Column Behavior

- Status: colored dot + text (Running green, Paused yellow, Stopped gray, Error red)
- MTM: green if positive, red if negative, neutral white at 0
- SL/Target: editable if strategy allows live edits
- Duration: HH:MM:SS since start
- Updated: time of last refresh

---

## Interactions

- Double click row -> open context panel
- Right click row -> context menu (Start/Pause/Modify/Stop/Delete)
- Inline edits -> only for SL/Target, validate before apply
- Filters apply instantly without blocking UI

---

## Visual Style

- High contrast dark surface (consistent with terminal style)
- Minimal borders, strong row separation
- No animations beyond subtle state change highlight

---

## Accessibility

- Keyboard shortcuts: Start (S), Pause (P), Modify (M), Stop (X)
- Focusable controls for keyboard navigation

---

## UX Notes

- Avoid modal spam: only use dialogs for Create/Modify
- Avoid auto popups on error; show in row + context panel
