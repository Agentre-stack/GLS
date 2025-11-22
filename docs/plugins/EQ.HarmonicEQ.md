# Harmonic EQ — EQ.HarmonicEQ

**Category:** EQ / Harmonics  
**Role:** Harmonic-aware bell EQ with selectable harmonic blends and mix control. Includes factory presets.

## Signal Flow
`Input → Base bell + harmonic overlay → Mix → Output`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `band_freq` | Band Freq | Center frequency of the bell. |
| `band_gain` | Band Gain | ±18 dB gain for the bell. |
| `band_q`    | Band Q    | Bandwidth/sharpness (0.2–10). |
| `harm_type` | Harm Type | Odd / Even / Hybrid harmonic blend. |
| `mix`       | Mix       | Dry/wet blend for the processed band. |
| `ui_bypass` | Soft Bypass | Goodluck footer bypass. |

## Usage Notes
- Use Odd for more forward, buzzy emphasis; Even for smoother shimmer; Hybrid for a balanced lift.
- Band Q ranges from broad tone moves (0.2) to narrow resonance control (10).
- Combine modest gain (+1 to +3 dB) with Hybrid for bus polish; push gain higher with Even for sweet vocal air.
- Presets: **Vocal Air**, **Synth Shine**, **Master Glue** for quick starting points.

## Known Limitations
- Single-band processor; additional bands would require stacking instances.
- No output trim/oversampling; avoid extreme boosts on dense material.

## Phase 2/3 TODOs
- [x] Provide factory presets for vocals, synths, and mix bus sweetening.
