# Tape Crush — GRD.TapeCrush

**Category:** GRD Saturation  
**Role:** Lo-fi tape crunch with wow/flutter, hiss, tone control, and mix.

## Signal Flow

`Input -> Modulated tape delay -> Hiss injection -> Saturation + tone filter -> Mix/trim -> Output`

## Parameters
- `drive`: Pushes the tape stage harder.
- `wow`: Slow pitch wobble depth.
- `flutter`: Faster modulation depth.
- `hiss`: Adds noise to the repeats.
- `tone`: Low-pass frequency after saturation.
- `mix`: Dry/wet blend.
- `output_trim`: Final gain (dB).

## Usage Notes
- Use wow ~0.3 and flutter ~0.2 to make loops feel recapped and alive.
- Lower tone when drive is high to keep the grit from overtaking cymbals or vox.
