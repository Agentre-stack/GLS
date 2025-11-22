# Wide Track — MDL.WideTrack

**Category:** Mod/Delay/LFO  
**Role:** Stereo widener with delay spread, HF preservation, mono-sum safety, and output trim.

## Signal Flow

`Input -> Mid/Side split -> Side delay + HF blend -> Width/mono safety -> Output trim -> Output`

## Parameters
- `width`: Gain applied to the side signal (0–200%).
- `delay_spread`: Extra delay (ms) applied to the side component.
- `hf_preserve`: Blends original undelayed side back in to keep highs intact.
- `mono_safe`: Reduces width >100% when engaged to maintain mono compatibility.
- `output_trim`: Final gain (dB) after widening.

## Usage Notes
- Add 1–2 ms of delay spread for Haas-style widening, then lift HF Preserve above 0.7 to avoid smear.
- Flip Mono Safe on when mastering stems so extreme width settings fold down cleanly.
