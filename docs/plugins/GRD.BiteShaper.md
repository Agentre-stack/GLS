# Bite Shaper — GRD.BiteShaper

**Category:** GRD Saturation  
**Role:** Bite-style waveshaper with foldback grit, tone control, and blend.

## Signal Flow

`Input -> Bite pre-gain -> Fold/waveshape core -> Tone low-pass -> Mix/trim -> Output`

## Parameters
- `bite`: Amount of pre-gain into the shaper.
- `fold`: Introduces wavefolding for asymmetric harmonics.
- `tone`: Low-pass cutoff after shaping (Hz).
- `mix`: Dry/wet blend.
- `output_trim`: Final gain (dB).

## Usage Notes
- Set Bite around 0.6 with Fold near 0.3 for guitar crunch that still responds to dynamics.
- Lower Tone to ~2 kHz to tame the foldback fizz when processing buses or full mixes.
