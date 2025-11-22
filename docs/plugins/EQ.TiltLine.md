# Tilt Line — EQ.TiltLine

**Category:** EQ / Tone  
**Role:** Classic tilt EQ with extra low/high shelves and pivot control. Includes factory presets.

## Signal Flow
`Input → Low shelf → High shelf → Output trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `tilt` | Tilt | Sets opposing boost/cut between lows and highs. |
| `pivot_freq` | Pivot Freq | Frequency about which the tilt rotates. |
| `low_shelf` | Low Shelf | Additional low shelf gain. |
| `high_shelf` | High Shelf | Additional high shelf gain. |
| `output_trim` | Output Trim | Gain staging. |

## Usage Notes
- Use Tilt for broad balance, then fine tune with Low/High Shelf to keep fundamentals or air steady.
- Pivot around 1 kHz for vocal/instrument busses; lower pivots emphasize body.
- Output Trim keeps comparisons level-matched.
- Presets: **Bright Vocal**, **Warm Bus**, **Air Lift** for quick starting points.

## Known Limitations
- Shelves share fixed Q (0.707) and cannot be bypassed individually; no dedicated bypass switch.

## Phase 2/3 TODOs
- [ ] Add shelf Q and bypass controls.
- [x] Provide preset macros (Bright Vocal, Warm Bus, Air Lift).
