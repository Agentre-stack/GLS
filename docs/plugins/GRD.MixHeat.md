# Mix Heat — GRD.MixHeat

**Category:** Saturation/Distortion  
**Role:** Mix-safe saturation with tone control.

## Signal Flow

`Input trim → Character shaper → Nonlinear drive → Tone → Mix/output trim`

## Parameters

| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `mode`        | Mode         | Clean / Tape / Triode characters. |
| `drive`       | Drive        | Amount of saturation. |
| `tone`        | Tone         | Sweeps HF emphasis. |
| `mix`         | Mix          | Dry/wet. |
| `input_trim`  | Input Trim   | Gain into the stage (-18 to +18 dB). |
| `output_trim` | Output Trim  | Final gain (-18 to +18 dB). |
| `ui_bypass`   | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Use Clean mode on mix bus for subtle glue, Tape/Triode for drums/guitars.
- Negative tone keeps cymbals smooth; positive adds presence.
- Goodluck cockpit with drive/tone macros on top and trims in the footer row.
- Presets: **Clean Glue**, **Tape Heat**, **Triode Push**.

## Known Limitations
- Tone filter is simple 1-pole; add shelving network later.
- Needs oversampling switch to handle hot material.

## Phase 2/3 TODOs
- [ ] Implement HQ oversampling + auto gain.
- [ ] Add VU metering.
- [ ] Provide bus/master presets.
