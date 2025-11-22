# Dyn Band — EQ.DynBand

**Category:** EQ / Dynamics
**Role:** Dual dynamic EQ bands for surgical boost/cut tied to signal level.

## Signal Flow
`Input → Dual band-pass detectors → Level envelopes → Gain computer → Wet/dry mix`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `input_trim` | Input Trim | Stage level into the bands (-18 to +18 dB). |
| `band1_freq` | Band 1 Freq | Center frequency of band 1 (80 Hz – 12 kHz). |
| `band1_q` | Band 1 Q | Bandwidth of band 1 (0.2 – 10). |
| `band1_thresh` | Band 1 Thresh | Threshold in dB to trigger gain changes. |
| `band1_range` | Band 1 Range | Gain range (positive = upward, negative = downward). |
| `band2_freq` | Band 2 Freq | Center frequency of band 2. |
| `band2_q` | Band 2 Q | Bandwidth of band 2. |
| `band2_thresh` | Band 2 Thresh | Threshold for band 2. |
| `band2_range` | Band 2 Range | Range for band 2. |
| `mix` | Mix | Blend between processed and dry. |
| `output_trim` | Output Trim | Final level match (-18 to +18 dB). |
| `ui_bypass` | Soft Bypass | Goodluck footer bypass. |

## Usage Notes
- Set both bands to complementary roles (e.g., upward boost on air, downward notch on mud).
- Use small negative Range values (-3 to -6 dB) for transparent de-essing; positive ranges can add dynamic sparkle.
- Mix control lets you parallel blend for subtlety; trims help level-match presets.
- Goodluck cockpit: dual-band macros up top, mix + trims in the footer row, logo/footer bypass.
- Presets: **De-Ess Air**, **Mud Tamer**, **Dynamic Sparkle**.

## Known Limitations
- Shared attack/release values are hard-coded (10 ms / 120 ms) for now.
- No per-band bypass switch; set Range to 0 dB to disable a band.

## Phase 2/3 TODOs
- [ ] Expose attack/release parameters per band.
- [ ] Add per-band listen and bypass buttons.
- [ ] Implement oversampling/linear-phase option for mastering cases.
