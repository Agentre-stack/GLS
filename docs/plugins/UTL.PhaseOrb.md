# PhaseOrb — UTL.PhaseOrb

**Category:** Utility / Stereo  
**Role:** Stereo “orbiter” that rotates the mid/side vector with LFO depth, tilt, and the Goodluck cockpit (logo header, hero orbit visual, footer trims/mix/bypass).

## Signal Flow
`Input Trim → Mid/Side matrix → Width + Tilt → Phase rotation (static + LFO) → Mix → Output Trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `width` | Width | Scales side channel prior to rotation (0 = mono, >1 = extra wide). |
| `phase_shift` | Phase Shift | Static rotation in degrees applied to the mid/side pair. |
| `orb_rate` | Orb Rate | LFO rate for orbiting the phase rotation (0.05–5 Hz). |
| `orb_depth` | Orb Depth | Depth of the LFO modulation (0 = static, 1 ≈ ±170°). |
| `tilt` | Tilt | Balances energy between mid and side (dB). |
| `mix` | Mix | Crossfade between dry signal and orbited field. |
| `output_gain` | Output Gain | Pre-trim to compensate before the footer Output Trim. |
| `input_trim` | Input Trim | Pre-processing gain (-24…+24 dB). |
| `output_trim` | Output Trim | Post-processing trim (-24…+24 dB). |
| `ui_bypass` | Soft Bypass | Cockpit bypass for host automation/A-B. |

## Usage Notes
- Small `phase_shift` values (~15°) with Width ≈ 1.2 can bring fuzzy mono stems back to life without obvious modulation; raise `orb_depth` for subtle drift.
- Use the hero orbit visual to confirm the LFO depth stays inside your correlation comfort zone; when the dot hugs the ellipse, energy is balanced.
- Input/Output trims plus Soft Bypass let you automate orbital moments on aux buses without hiccups.
