# Octane Clipper — GRD.OctaneClipper

**Category:** GRD Saturation  
**Role:** Multi-curve clipper with drive, clip type morphing, HP filter, and mix control.

## Signal Flow

`Input -> High-pass drive prep -> Clip curve (hard/tanh/expo) -> Mix/trim -> Output`

## Parameters
- `drive`: Gain into the clipper.
- `clip_type`: 0 = hard, 1 = tanh, 2 = exponential soft clip.
- `hp`: High-pass cutoff before clipping (Hz).
- `mix`: Dry/wet blend for parallel clipping.
- `output_trim`: Final level trim (dB).

## Usage Notes
- Hard clip mode (clip_type ~0) nails peaks for drum buses; exponential (~2) is smoother for synths.
- Raise the HP around 80 Hz to keep low-end punch when clipping mix-bus transients.
