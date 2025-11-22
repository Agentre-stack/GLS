# NoiseGenLab — UTL.NoiseGenLab

**Category:** Utility / Generator  
**Role:** Programmable broadband noise injector with burst envelopes, stereo variance, and the full Goodluck cockpit (logo header, hero “noise energy” visual, footer trims/mix/bypass).

## Signal Flow
`Input Trim → Noise engine (color selector) → Burst envelopes + stereo variance → HP/LP filters → Mix → Output Trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `noise_color` | Noise Color | White, Pink, or Brown coloration before filtering. |
| `noise_level` | Noise Level | Level of the generated noise before it’s mixed (dB). |
| `mix` | Mix | Crossfade between dry input and noise-augmented signal. |
| `density` | Density | Controls how often burst envelopes retrigger (0 = sparse, 1 = smooth). |
| `low_cut` | Low Cut | High-pass filter on the generated noise (20–4 kHz). |
| `high_cut` | High Cut | Low-pass filter on the generated noise (1–20 kHz). |
| `stereo_var` | Stereo Variance | Random decorrelation and burst-length deviation per channel. |
| `input_trim` | Input Trim | Pre-noise gain staging (-24…+24 dB). |
| `output_trim` | Output Trim | Post blend trim (-24…+24 dB). |
| `ui_bypass` | Soft Bypass | Cockpit bypass for instant A/B without resetting burst envelopes. |

## Usage Notes
- Keep `noise_level` conservative (e.g., -30 dB) and raise `mix` for subtle grit; use White + high `density` for tape hiss, or Brown + low `density` for thunderous bursts.
- `stereo_var` plus moderate `density` turns the hero visual’s bar into a pulsing motion—great for granular swells under pads.
- Because Input/Output trims and Soft Bypass live in the footer, you can audition NoiseGenLab on a parallel bus while keeping level-matched stems.
