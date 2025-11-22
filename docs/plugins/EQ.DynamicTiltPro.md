# Dynamic Tilt Pro — EQ.DynamicTiltPro

**Category:** EQ / Tone  
**Role:** Tilt EQ that reacts to program level for automatic brightness vs warmth balancing. Phase‑3 upgrades deliver the Goodluck cockpit UI, RMS/peak detection modes, shelf “Styles”, and footer trims/dry-wet/bypass.

## Signal Flow
`Input Trim → Envelope/RMS follower → Detector Mode/Threshold → Dynamic tilt calculator → Linked low/high shelves (Style) → Mix → Output Trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `tilt` | Tilt | Base tilt amount (-18 to +18 dB). |
| `range` | Range | Additional tilt applied once the detector crosses the threshold. |
| `thresh` | Threshold | Level in dBFS at which dynamic action begins. |
| `pivot_freq` | Pivot | Pivot frequency for the tilt (80 Hz – 12 kHz). |
| `detector_mode` | Detector | Peak or RMS envelope follower. |
| `shelf_style` | Shelf Style | Classic (0.707 Q), Wide (gentler 0.5 Q), Tight (sharper 1.2 Q). |
| `attack` | Attack | Envelope attack in ms. |
| `release` | Release | Envelope release in ms. |
| `input_trim` | Input Trim | Pre-tilt gain staging (-24 to +24 dB). |
| `mix` | Mix | Dry/wet blend for subtle tilt reinforcement. |
| `output_trim` | Output Trim | Final gain staging (-12 to +12 dB). |
| `ui_bypass` | Soft Bypass | Goodluck footer bypass for instant A/B comparisons. |

## Usage Notes
- Use Tilt to set the static tone, then Range + Threshold to describe how aggressively it should react to loud moments.
- Switch the Detector to RMS for smoother mastering moves, or Peak for snappier vocal/perc shaping.
- Shelf Style alters the character of the linked low/high shelves; “Tight” locks energy near the pivot while “Wide” wraps more of the spectrum.
- The teal hero display shows the live tilt curve, envelope level, and threshold so you can see what the dynamics engine is doing.
- Attack/Release let you dial how quickly the tone rebalances—use slower release for mix bus smoothing, faster times for transient tone chasing.

## Known Limitations
- Detector still listens to the loudest channel; mid/side weighting remains future work.
- Factory presets cover Vocal Pop, Drum Bus, and Master Air starting points.

## Phase 2/3 TODOs
- [x] Offer RMS/peak switch plus weighting/style options.
- [x] Add Style buttons for different shelf shapes.
- [x] Provide snapshot presets (Vocal Pop, Drum Bus, Master Air).
