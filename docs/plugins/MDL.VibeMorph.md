# Vibe Morph — MDL.VibeMorph

**Category:** Mod/Delay/LFO  
**Role:** Multi-stage vibe/chorus hybrid with morphing all-pass stages.

## Signal Flow

`Input -> Cascaded all-pass stages (rate/depth/throb) -> Wet/dry mix -> Output`

## Parameters
- `rate`: Speed of the LFO driving the stage offsets.
- `depth`: Amount of modulation per stage.
- `throb`: Increases resonance/phase emphasis per stage.
- `mode`: Choose between 4-stage Vibe or 6-stage Chorus voicing.
- `mix`: Wet/dry balance.

## Usage Notes
- For chewy uni-vibe textures set Mode=Vibe, rate ~4 Hz, depth 0.7, throb 0.5.
- Mode=Chorus adds two more stages for wider pads; keep mix below 0.6 to stay mono-safe.
