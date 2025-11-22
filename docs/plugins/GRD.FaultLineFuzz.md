# Fault Line Fuzz — GRD.FaultLineFuzz

**Category:** Saturation/Fuzz  
**Role:** Biasable fuzz with input trim, envelope gate, tone sweep, and output trim.

## Signal Flow

`Input trim → Bias/drive stage → Gate (envelope follower) → Tone LPF → Output trim`

## Parameters

| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `input_trim`  | Input Trim   | ±24 dB pre-gain. |
| `fuzz`        | Fuzz         | Amount of drive into the fuzz core. |
| `bias`        | Bias         | Offsets the clipping stage for asymmetry. |
| `gate`        | Gate         | Sets threshold/release on the envelope-controlled gate. |
| `tone`        | Tone         | 400 Hz–12 kHz low-pass after fuzz. |
| `output_trim` | Output Trim  | ±24 dB post gain. |
| `ui_bypass`   | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Use Bias to make octave-up tones (positive) or heavy bass distortions (negative).
- Gate near 0.2 for sustained leads, higher for sputtery textures.
- Presets: **Vocal Edge**, **Gritty Bass**, **Alt Drum Crush**.

## Known Limitations
- Envelope gate is single-pole; may pump on bass-heavy signals.

## Phase 2/3 TODOs
- [ ] Add optional oversampling toggle.
- [ ] Provide diode model choices for different fuzz flavours.
