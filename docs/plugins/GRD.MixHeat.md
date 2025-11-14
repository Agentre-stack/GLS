# Mix Heat — GRD.MixHeat

**Category:** Saturation/Distortion  
**Role:** Mix-safe saturation with tone control.

## Signal Flow

`Input -> Character shaper -> Nonlinear drive -> Tone -> Output`

## Parameters

| ID       | Display Name | Notes |
|----------|--------------|-------|
| `mode`   | Mode          | Clean / Tape / Triode characters. |
| `drive`  | Drive         | Amount of saturation. |
| `tone`   | Tone          | Sweeps HF emphasis. |
| `mix`    | Mix           | Dry/wet. |
| `output` | Output Trim   | Final gain. |

## Usage Notes
- Use Clean mode on mix bus for subtle glue, Tape/Triode for drums/guitars.
- Negative tone keeps cymbals smooth; positive adds presence.

## Known Limitations
- Tone filter is simple 1-pole; add shelving network later.
- Needs oversampling switch to handle hot material.

## Phase 2/3 TODOs
- [ ] Implement HQ oversampling + auto gain.
- [ ] Add input trim + VU metering.
- [ ] Provide bus/master presets.
