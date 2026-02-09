# Strategy Manager V2 - Mechanism

Date: 2026-02-09
Goal: Deterministic lifecycle control with low-latency, low-variance UI updates.

---

## Core Principles

- Service owns truth, UI reflects truth.
- Updates are batched at fixed cadence (250-500ms).
- No tick-by-tick repaint.
- Emit only when values change.

---

## Event Flow

1) Data feed updates prices (FeedHandler).
2) StrategyService computes MTM and counters on cadence.
3) StrategyService emits instanceUpdated signals.
4) StrategyTableModel applies delta updates.
5) Proxy model filters and sorts.
6) Table view repaints only changed rows.

---

## State Machine

Created -> Running -> Paused -> Running -> Stopped -> Deleted

Rules:
- Start only from Created or Stopped.
- Pause only from Running.
- Resume only from Paused.
- Stop from Running or Paused.
- Delete only from Stopped.
- Error is a parallel state flag, not a primary state.

---

## Risk Gates

- Start denied if feed is stale.
- Start denied if account mismatch.
- Modify denied if parameter is locked while running.

---

## Data Model

Per instance:
- Identity: instanceId, name, type, symbol, account, segment
- State: lifecycle, lastError
- Metrics: mtm, sl, target, entry, qty, positions, orders, duration
- Params: key-value map
- Timestamps: created, lastUpdated, lastStateChange

---

## Persistence (Optional)

- Soft delete only (deleted flag).
- Store parameters as compact JSON.
- Load on startup and reconcile with live state.

---

## Concurrency

- UI thread owns Qt models.
- Service timer runs on UI thread but uses minimal work.
- Heavy analytics or backtests must run off-thread.
