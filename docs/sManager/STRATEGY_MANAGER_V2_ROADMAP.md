# Strategy Manager V2 - Roadmap

Date: 2026-02-09
Goal: Deliver a production-grade Strategy Manager in stages.

---

## Phase 1 - MVP (1-2 weeks)

- Create, start, pause, resume, modify, stop, delete
- Table UI with core columns
- Basic filters (type, status)
- Service-level cadence updates

---

## Phase 2 - Stability (1-2 weeks)

- Error states and audit trail
- Persistence (optional)
- Improved table performance for 200+ rows
- Live edit validation

---

## Phase 3 - Risk & Control (2 weeks)

- Kill switch (global)
- Risk guard (max loss, max orders)
- Shadow mode (dry run)

---

## Phase 4 - Advanced (ongoing)

- Strategy bundles
- Presets library
- Replay mode
- Performance snapshots
- Alert rules

---

## Done Definition

- No UI lag under 10 updates per second for 200 rows
- All lifecycle transitions are deterministic
- Errors are recoverable and visible
- Filters never block interaction
