# Competitor-Level Hardening & Upgrade Plan — GLS Suite

This plan documents the concrete solutions, acceptance checks, and cadence for taking the Phase 3-complete GLS suite to a hardened, competitor-level release. It pairs the existing two-by-two delivery rhythm with automation, visual regression, UX consistency, and documentation guarantees.

## Read This First
- **Baseline:** Phase 3 cockpit/DSP/preset sweep is done (see `docs/PHASE3_SUMMARY.md` and `docs/BUILD_STATUS.md`). The steps below assume the current validators/build scripts remain green.
- **Definition of "hardened":** Every SKU must pass automated build + validator, visual-regression checks for the Goodluck cockpit, consistent UX components, and updated docs/presets for any user-visible change.
- **Cadence:** Keep the two-by-two rotation for implementation, but add nightly CI for whole-suite guardrails.

## Plan of Attack (Solutions & DoD)

### 1) Automated QA & Release Hardening
- **CI steps:** `cmake --build GLS_Project/Builds/UnixMakefiles --target all -- -j8`, `scripts/collect_vst3.sh`, `scripts/validate_vst3.sh` with logs archived per run.
- **Gates:** Fail the pipeline on new validator warnings/errors; surface Steinberg informational noise separately to avoid masking regressions.
- **Artifacts:** Upload `GLS_Project/Builds/VST3_All` plus validator logs to CI artefacts for QA handoff and trend tracking.

### 2) Visual Regression & Branding Enforcement
- **Baseline capture:** Take canonical screenshots for each SKU showing the Goodluck cockpit (header/footer/logo, macro/micro layout, meters).
- **Checks:** Add pixel-diff CI job with tolerance tuned for font/render jitter; flag missing headers/footers, misaligned labels, or meter placement changes.
- **Scope:** Apply to all plugins listed in `docs/BUILD_STATUS.md`; add new SKUs to the screenshot suite on creation.

### 3) UX Consistency & Shared Components
- **Component library:** Centralize JUCE widgets for meters (GR/LUFS/scope), macro/micro controls, selectors, and preset browsers; forbid bespoke duplicates.
- **Usage contracts:** Document props/skins and enforce via lint/checks in the shared module; add quick usage examples to `docs/GOODLUCK_UI_GUIDE.md`.
- **Cross-plugin adoption:** Track adoption per SKU; block merges if new UI bypasses the shared kit.

### 4) Workflow, Profiles, and Preset Ops
- **Profile helpers:** Ship session-aware capture/transport helpers (Ambience Evolver multi-profile, Signal Tracer tap labels) with metadata stored in plugin state.
- **Preset standardization:** Normalize tags/genres and author/version metadata; add preset-pack manifests to speed host browsing.
- **User flows:** Add in-DAW “tour” overlays for first launch to explain cockpit regions and presets.

### 5) Documentation & Onboarding Guarantees
- **Change logging:** Any DSP/UI/preset change updates the relevant `docs/plugins/*.md` entry plus `CHANGELOG.md` with date and SKU.
- **Runbooks:** Extend `docs/PHASE3_SUMMARY.md` with validator and screenshot run links; keep a `pluginval_reports/` folder for the latest sweep.
- **Support readiness:** Maintain a short FAQ/troubleshooting appendix per SKU for known validator notes and host-scan quirks.

### 6) License Integrity & Anti-Piracy Protections
- **Build signatures:** Sign VST3 bundles and installers; verify signatures during CI packaging and in installer pre-flight to stop tampered binaries.
- **Watermarking:** Add per-build watermarking (user/seat keyed) to presets and DSP constants where feasible to trace leaked binaries or packs.
- **Online checks:** Include an opt-in lightweight entitlement ping with offline grace periods; cache encrypted tokens to avoid blocking creative sessions.
- **Telemetry hygiene:** Log only the minimum needed for entitlement/anti-abuse (e.g., anonymous build ID, SKU, timestamp), with GDPR-friendly retention.
- **Tamper detection:** Add checksum verification on load and surface a clear error if modified binaries or resource packs are detected.

### 7) Risk Controls & Red-Team Checks
- **Stale bundle detection:** Add checksum or timestamp diff in CI to ensure `collect_vst3.sh` ran; fail if `VST3_All` is older than build artefacts.
- **Host coverage:** Periodically run validator and smoke tests on at least two hosts (pluginval + a DAW) to catch host-specific behaviors.
- **Chaos drills:** Intentionally break a shared component or preset manifest in a branch to ensure CI and visual-regression gates catch the issue.

## Execution Cadence & Hand-off
1. Pick two SKUs → implement items from sections 3–5 → rebuild → collect → validate → screenshot diff.
2. If green, update `docs/BUILD_STATUS.md` and relevant plugin docs; attach CI artefacts (validator logs + screenshots) to the run.
3. Nightly CI runs full-suite build/collect/validate/screenshot diff; triage failures next morning.
4. On release, publish artefacts, validator reports, and a brief UX delta summary in `CHANGELOG.md`.
