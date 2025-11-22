# Sub Harm Forge — GRD.SubHarmForge

**Category:** GRD Saturation  
**Role:** Subharmonic enhancer that synthesizes subs from lows with drive/blend control.

## Signal Flow

`Input -> Low & sub filters -> Rectified synth sine -> Mix with drive -> Blend/trim -> Output`

## Parameters
- `depth`: Amount of synthesized subharmonic.
- `crossover`: Frequency (Hz) where subs are generated.
- `drive`: Pushes the forged subs into saturation.
- `blend`: Dry/wet mix of the forged tone.
- `output_trim`: Final gain (dB).

## Usage Notes
- Use lower crossover (~60 Hz) on kick/bass; higher (~100 Hz) for synth stacks.
- Blend <0.6 for natural reinforcement; crank depth + drive for special FX.
