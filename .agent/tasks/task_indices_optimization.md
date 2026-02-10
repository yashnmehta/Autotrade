# Task: Optimize Indices View & Style Title Bar

## Status
- [x] Analyze performance of Indices View initiation.
- [x] Stagger `IndicesView` creation using `QTimer::singleShot`.
- [x] Implement token-based O(1) lookups for UDP index updates.
- [x] Use `QSet` for efficient index selection management.
- [x] Refactor `AllIndicesWindow` to be created on-demand.
- [x] Fix index update issue by restoring name-based fallback for NSE indices.
- [ ] Update active window title bar color to match dark theme.
- [ ] Verify fix for index price updates.

## Context
The user identified that the Indices View was slowing down startup. Optimization was implemented but led to a bug where updates stopped (due to missing token mapping for NSE). The user also wants to change the active window title bar color from blue to a color that fits the dark theme.

## Next Steps
1. Apply the style changes to `CustomTitleBar.cpp`.
2. Re-build and verify both index updates and title bar style.
