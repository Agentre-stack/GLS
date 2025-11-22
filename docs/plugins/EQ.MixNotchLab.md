# Mix Notch Lab — EQ.MixNotchLab

**Category:** EQ / Surgical  
**Role:** Surgical dual notch EQ with preview/listen modes for mix cleanup. Includes factory presets.

## Signal Flow
`Input → Notch 1 → Notch 2 (with preview taps) → Output`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `notch1_freq`  | Notch 1 Freq  | Center frequency of notch 1. |
| `notch1_q`     | Notch 1 Q     | Q/bandwidth of notch 1. |
| `notch1_depth` | Notch 1 Depth | Attenuation depth for notch 1. |
| `notch2_freq`  | Notch 2 Freq  | Center frequency of notch 2. |
| `notch2_q`     | Notch 2 Q     | Q/bandwidth of notch 2. |
| `notch2_depth` | Notch 2 Depth | Attenuation depth for notch 2. |
| `listen_mode`  | Listen Mode   | Stereo/Main/Diff preview modes. |
| `ui_bypass`    | Soft Bypass   | Goodluck footer bypass. |

## Usage Notes
- Use Listen Mode to audition the removed content (Diff) or isolate a notch target. Start with moderate Q (4–6) and depth (8–12 dB).
- Pair a lower notch with a higher one for boxy + fizz removal on full mixes; tighten Q for resonant squeaks.
- Presets: **Vocal Clean**, **Drum Box Cutter**, **Mix Fizz Tamer** for quick starting points.

## Known Limitations
- Q and depth are static per instance; dynamic notch modes remain a future idea.
- No per-notch bypass; set depth to 0 dB to disable a band.

## Phase 2/3 TODOs
- [x] Provide preset banks for drums/vocals/mix cleanup.
