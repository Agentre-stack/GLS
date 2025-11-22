# Ghost Echo — MDL.GhostEcho

**Category:** Mod/Delay/LFO  
**Role:** Diffuse tape echo with blur noise, damping, and stereo width.

## Signal Flow

`Input -> Delay + damping filter -> Blur noise injection -> Feedback -> Width mix -> Output`

## Parameters
- `time`: Delay time (ms).
- `feedback`: Regeneration amount.
- `blur`: Adds wandering noise for ghosted repeats.
- `damping`: Low-pass filtering per repeat.
- `width`: Stereo widening of the wet signal.
- `mix`: Wet/dry blend.

## Usage Notes
- Blur plus damping creates hazy pad reverbs; keep feedback <0.6 to avoid runaway.
- Width >1.5 makes ghost repeats wrap around extremes—check mono compatibility.
