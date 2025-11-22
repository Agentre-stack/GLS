# Phase Grid — MDL.PhaseGrid

**Category:** Mod/Delay/LFO  
**Role:** Multi-stage stereo phaser with animated grid modulation.

## Signal Flow

`Input -> Parallel all-pass stages per channel -> Feedback blend -> Wet/dry mix -> Output`

## Parameters
- `stages`: Selects 2–12 cascaded all-pass stages for deeper sweeps.
- `center_freq`: Base frequency of the grid sweep (Hz).
- `rate`: LFO speed (Hz) driving per-stage offsets.
- `depth`: Modulation depth applied to the centre frequency.
- `feedback`: Positive/negative feedback for resonant notches.
- `mix`: Crossfades between dry and effected signals.

## Usage Notes
- Higher stage counts plus feedback (>0.4) give resonant comb-like textures.
- Subtle depth + slow rate keeps stereo image moving without sounding obvious.
