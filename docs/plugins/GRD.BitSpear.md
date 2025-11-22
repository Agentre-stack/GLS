# Bit Spear — GRD.BitSpear

**Category:** GRD Saturation  
**Role:** Digital bitcrusher/decimator with drive, mix, and trim controls.

## Signal Flow

`Input trim → Downsample hold → Bit depth quantizer → Waveshaper drive → Mix/trim → Output`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `input_trim` | Input Trim | Gain into the crusher (-18 to +18 dB). |
| `bits` | Bits | Quantization depth (4–16 bits). |
| `downsample` | Downsample | Number of samples to hold (pseudo sample-rate reduction). |
| `drive` | Drive | Pushes the crushed signal into a saturation stage. |
| `mix` | Mix | Dry/wet balance for parallel grit. |
| `output_trim` | Output Trim | Final level trim (-18 to +18 dB). |
| `ui_bypass` | Soft Bypass | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Use small downsample values (2–4) for vocal harmonics; extreme values create classic digital aliasing.
- Combine 8-bit mode with 40–60% mix to add edge while keeping low-level detail intact.
- Goodluck cockpit: crusher macros up top, trims/bypass in the footer row.
- Presets: **Lo-Fi Vox**, **Bit Snare**, **8-Bit Lead**.
