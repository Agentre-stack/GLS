# MS Matrix — UTL.MSMatrix

**Category:** Utility / Stereo  
**Role:** Mid/side gain, width, and tonal filtering cockpit for folding lows to mono, widening highs, and checking polarity via the Goodluck layout (logo header, hero width visual, footer trims/mix/bypass).

## Signal Flow
`Input Trim → Mid/Side encode → Side HPF/LPF + polarity → Mid/Side gain + width/mono fold → Mid/Side decode → Mix → Output Trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `mid_gain`    | Mid Gain | Mid-channel trim (-24…+12 dB). |
| `side_gain`   | Side Gain | Side-channel trim (-24…+12 dB). |
| `width_pct`   | Width % | Scales the side channel from 0–200% (0 = mono, 200 = extreme width). |
| `mono_fold`   | Mono Fold | Crossfades the output toward mono, useful for checking compatibility or tightening bass. |
| `side_hpf`    | Side HPF | High-pass filter applied to the side channel (20–800 Hz). |
| `side_lpf`    | Side LPF | Low-pass filter applied to the side channel (2–20 kHz). |
| `phase_mid`   | Phase Mid | Flips the polarity of the mid channel. |
| `phase_side`  | Phase Side | Flips the polarity of the side channel. |
| `mix`         | Mix | Dry/wet blend for parallel width experiments. |
| `input_trim`  | Input Trim | Pre-matrix trim (-24…+24 dB). |
| `output_trim` | Output Trim | Post-matrix trim (-24…+24 dB). |
| `ui_bypass`   | Soft Bypass | Cockpit-level bypass for comparison and automation without touching the plugin enable. |

## Usage Notes
- Use Side HPF to keep sub information centered before adding width; set LPF around 8–12 kHz to let only air frequencies spread wide.
- Mono Fold at ~0.2 quickly shows you how the mix collapses to mono; automate it to create breakdown effects.
- The hero visual shows Mid vs Side energy plus a live width needle so you can stop widening once the meter hangs near the outer arcs.
- Phase Mid/Side toggles sit with the filters so you can fix odd polarity issues before hitting the mix bus.

## Known Limitations
- Metering is block-based (smoothing ~15%), so extremely fast modulation won’t show every transient but keeps the UI quiet.

## Phase 2/3 TODOs
- [x] Goodluck cockpit UI with hero width scope, macro/micro layout, footer trims/mix/bypass.
- [x] DSP additions (HPF/LPF on side, polarity toggles, mono fold, mix).
- [ ] Optional preset pack (Bus Widen, Vocal Center, Lo-Fi Mono) still on the preset sprint backlog.
