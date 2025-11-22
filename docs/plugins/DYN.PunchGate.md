# Punch Gate — DYN.PunchGate

**Category:** Dynamics / Gate  
**Role:** Transient-friendly gate with punch boost, sidechain filtering, and the full Goodluck cockpit (logo header, teal gate visual, footer trims/dry-wet/bypass).

## Signal Flow
`Input Trim → Sidechain HPF/LPF → Envelope follower → Gate logic + hold/hysteresis → Punch boost → Mix → Output Trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `thresh` | Threshold | Gate opening threshold (dB). |
| `range` | Range | Amount of attenuation when closed. |
| `attack` | Attack | Attack time (ms). |
| `hold` | Hold | Hold time after opening (ms). |
| `release` | Release | Release time (ms). |
| `hysteresis` | Hysteresis | Gap between open/close thresholds. |
| `punch_boost` | Punch | Adds transient boost when gate triggers. |
| `sc_hpf`      | SC HPF | High-pass filter for the detector/sidechain input. |
| `sc_lpf`      | SC LPF | Low-pass filter for the detector/sidechain input. |
| `mix`         | Blend  | Crossfade between processed and dry signal. |
| `input_trim`  | Input  | Pre-gate gain staging (-24…+24 dB). |
| `output_trim` | Output | Post chain trim (-24…+24 dB). |
| `ui_bypass`   | Soft Bypass | Cockpit bypass for instant A/B without resetting the detector. |

## Usage Notes
- Set Hysteresis to ~6 dB to avoid chatter on drums; increase Hold for sustained guitars.
- Punch adds transient emphasis; useful on toms/snare to keep attack.
- Engage the sidechain bus in your DAW to key the gate from kick/snare tracks and use the SC HPF/LPF sliders to focus the detector.
- Input/Mix/Output live in the Goodluck footer so you can level-match quickly while using the teal gate meter for visual confirmation.

## Known Limitations
- None noted; presets cover Drum, Vox, and Guitar starting points.

## Phase 2/3 TODOs
- [x] Add external sidechain and filter controls.
- [x] Visual meters for gate state and punch boost.
- [x] Ship preset set for drums, guitars, vocals.
