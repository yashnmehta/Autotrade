# Implementation Plan: Title Bar Styling & Index Update Fix Verification

## Objective
Optimize the Indices View (performance) and style the active window title bar to match the dark theme. Ensure index price updates are working correctly after recent changes.

## Proposed Changes

### 1. Title Bar Styling (`CustomTitleBar.cpp`)
- Change the active title bar background from `#005A9C` (Blue) to a darker shade like `#2d2d2d`.
- Adjust button hover colors to match the new dark scheme.
- Ensure text remains readable (White for active, Gray for inactive).

### 2. Verify Index Update Fix
- Re-apply the `IndicesView` logic that handles both token-based and name-based lookups.
- Populate the `m_upperToDisplayName` cache during initialization.
- Resolve naming discrepancies in `UdpBroadcastService.cpp` (e.g., FO vs CM mappings).

## Verification Plan

### Automated Tests
- Run `benchmark_repository_filters` (if applicable) to ensure no regressions in data handling.

### Manual Verification
1. Launch the application.
2. Log in and wait for the Main Window to appear (verify staggered load).
3. Open the "Indices" window.
4. Verify that prices are updating in real-time (confirming name/token mapping works).
5. Check the title bar color of the active `Market Watch` or `Indices` window.
