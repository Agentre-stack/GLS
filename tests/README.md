# GLS Suite Test Plan (Phase 1)

Phase 1 focuses on scaffolding and build validation. Automated tests are light on purpose:

1. **Build smoke test**
   - Run `scripts/build_all_debug.sh` after every plugin wave.

2. **VST3 validation**
   - Run `scripts/validate_vst3.sh` to ensure Steinberg's `vst3validator` is clean.

3. **Manual DAW smoke test**
   - Load each new plugin in Reaper / Cubase.
   - Verify parameters respond and automation writes without crashes.

4. **Future JUCE UnitTests (Phase 2+)**
   - Add per-plugin `UnitTest` subclasses focusing on DSP math.
   - Tests will live under `tests/<Namespace>/<ProductName>/` as they grow.

Document issues + fixes in `docs/BUILD_STATUS.md` as you go.
