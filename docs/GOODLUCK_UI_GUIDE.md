# Goodluck UI / Aesthetic Build Guide (Phase 3)

Use this as the quick reference for the Goodluck cockpit look-and-feel so Phase 3 iterations stay consistent across the suite.

## Core Visual Language
- **Header/Footer:** Goodluck header with product + SKU, accent colour per family (GLS teal, GRD magenta, etc.). Footer hosts trims, mix, and soft bypass.
- **Layout:** Macro rotaries in the hero row, supportive controls in lower rows; labels centred above/below rotaries for clarity.
- **Colours/Fonts:** Goodluck palette from `ui/GoodluckLookAndFeel.h`; text uses `gls::ui::makeFont` helpers; avoid ad-hoc colours.
- **Meters/Visuals:** Prefer shared components (GR meters, orbit/hero visuals, scopes) rather than bespoke drawings; align meters with macro row when present.
- **Presets:** Host program list populated with 3–5 factory presets per plugin; names match docs.

## Interaction Patterns
- **Trims & Soft Bypass:** Input/output trims plus `ui_bypass` are standard in footer; bypass is a soft/processing toggle, not host bypass.
- **Macro vs Micro:** Macro controls (drive/smear/mix/etc.) sit in the first row; micro/support (filters, trims) live in a second row or side column.
- **Combo/Selector:** Use centred combo boxes for modes/profiles; justify content and keep widths consistent with rotaries.
- **Labeling:** Use `initSlider`/`initToggle` helpers to attach labels; keep text short and uppercase/lowercase consistent with docs.

## Implementation Checklist (per plugin)
1. Wire parameters: trims (input/output), `ui_bypass`, macros/micros per spec; presets registered in the host list.
2. Apply Goodluck look-and-feel: header/footer accent, setLookAndFeel, labels with `gls::ui::Colours`.
3. Layout: macro row first, supporting row beneath or split; place bypass in footer bounds; call `layoutLabels()` after `resized()`.
4. Docs: update `docs/plugins/<SKU>.md` with cockpit description, parameters, presets, and usage notes.
5. Build/Validate: `cmake --build ...`, `scripts/collect_vst3.sh`, `scripts/validate_vst3.sh`; address only new warnings.

## Quick Visual QA
- Header shows correct SKU name and accent colour.
- Footer contains trims, mix (if present), and soft bypass; meters/hero visuals align with macro row.
- Labels centred and readable; no ad-hoc colours/fonts.
- Preset list visible in host program selector and matches docs.

## Where to Test
- Aggregated VST3s for inspection and pluginval: `GLS_Project/Builds/VST3_All`.
- Expected validator noise: “Not all points read via IParameterChanges” (info only); some legacy silent-flag infos are non-fatal.
