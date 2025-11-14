# GLS Suite Changelog

## 2025-11-13 — Tooling & Pitch/Utility Wave
- Added CMake targets for ChannelPilot, ChannelStripOne, ChopperTrem, MixHeat, ShiftPrime, DoubleStrike, ShimmerFall, GrowlWarp, and SignalTracer.
- Introduced `scripts/build_all_debug.sh` and `scripts/validate_vst3.sh` for reproducible builds + vst3validator coverage.
- Added doc framework: `BUILD_STATUS.md`, `LESSONS_PHASE1.md`, per-plugin docs, and updated SWOT/SOP/SDO notes to reflect the new tooling gate.
- Scaffolded PIT.ShimmerFall, PIT.GrowlWarp, PIT.TimeStack, PIT.MicroShift processors/editors.

## 2025-11-12 — Core Wave
- Initial GLS namespace scaffolds (ChannelPilot, ChannelStripOne).
- Created `src/<Namespace>/<ProductName>/` directory convention and CSV authority file.
