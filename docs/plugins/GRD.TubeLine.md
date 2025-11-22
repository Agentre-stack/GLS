# Tube Line — GRD.TubeLine

**Category:** GRD Saturation  
**Role:** Tube-inspired asymmetrical saturator with bias, character, and mix controls.

## Signal Flow

`Input -> Input trim -> Bias/character shaping -> Mix/trim -> Output`

## Parameters
- `input_trim`: Gain before saturation.
- `bias`: Offsets asymmetry to emulate tube biasing.
- `character`: Morphs between soft and hard clipping.
- `mix`: Dry/wet balance.
- `output_trim`: Final gain (dB).

## Usage Notes
- Increase bias for more even-order harmonics; set character ~0.3 for smooth warmth.
- Parallel mix lets you add tube tone while keeping transient snap.
