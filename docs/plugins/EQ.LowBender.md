# Low Bender — EQ.LowBender

**Category:** EQ / Low-end sculpting  
**Role:** Shape subs, add punch, and tighten lows with sub shelf, peak, and variable high-pass. Includes factory presets.

## Signal Flow
`Input → Sub shelf → Punch peak → Tightness high-pass`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `sub_boost` | Sub Boost | ±12 dB of low shelf around ~55 Hz. |
| `low_cut` | Low Cut | High-pass cutoff (20–120 Hz). |
| `punch_freq` | Punch Freq | Center of the punch peak (60–400 Hz). |
| `punch_gain` | Punch Gain | ±12 dB of punch boost/cut. |
| `tightness`  | Tightness  | Controls Q of punch + high-pass. |

## Usage Notes
- Boost the sub shelf slightly (+2 to +4 dB) while adding a matching low cut for weight without mud.
- Tightness near 1 sharpens the punch band and high-pass, great for modern low-end control.
- Use punch cut with high tightness to notch boxy bass frequencies.
- Presets: **808 Lift**, **Bass Guitar**, **Kick Tight** for quick starts.

## Known Limitations
- Sub shelf frequency is fixed; future update should expose frequency control.
- No mix knob or output trim.

## Phase 2/3 TODOs
- [ ] Add output trim + wet/dry blend.
- [ ] Expose shelf frequency and additional punch band.
- [x] Provide presets for 808s, bass guitar, and kick drum alignment.
