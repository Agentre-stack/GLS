# Iron Bus — GRD.IronBus

**Category:** GRD Saturation  
**Role:** Bus glue clipper with drive, glue amount, high-pass, tilt, mix, and trim controls.

## Signal Flow

`Input -> HP filter -> Tilt EQ -> Drive/clipper -> Glue blend -> Mix/trim -> Output`

## Parameters
- `drive`: Pushes into the nonlinear stage.
- `glue`: Crossfades between clean and clipped signals before the mix stage.
- `hpf`: Pre-drive high-pass frequency (Hz) to protect lows.
- `tilt`: Broad tonal tilt (negative = darker, positive = brighter).
- `mix`: Dry/wet balance.
- `output_trim`: Final gain (dB).

## Usage Notes
- Set Glue around 0.4 with Drive 0.6 for cohesive bus saturation that still breathes.
- Tilt slightly positive (+0.2) after heavy bus drive to restore sheen without EQ downstream.
