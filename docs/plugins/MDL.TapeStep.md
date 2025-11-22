# Tape Step — MDL.TapeStep

**Category:** Mod/Delay/LFO  
**Role:** Wow/flutter tape delay with saturating feedback.

## Signal Flow

`Input -> Modulated delay line -> Tone filter -> Saturation/feedback -> Wet/dry mix -> Output`

## Parameters
- `time`: Delay time in milliseconds (20–2000 ms).
- `feedback`: Amount of regenerated signal.
- `drive`: Input level into the tape saturation stage.
- `wow`: Slow tape-speed wobble depth.
- `flutter`: Faster mechanical flutter depth.
- `tone`: Moves the low-pass tilt of the repeats.
- `mix`: Wet/dry blend.

## Usage Notes
- Use wow ~0.3 / flutter ~0.2 for lush chorusy repeats; push drive for compressed echoes.
- Keep feedback <0.8 when drive is high to avoid runaway oscillation.
