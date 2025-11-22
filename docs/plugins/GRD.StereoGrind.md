# Stereo Grind — GRD.StereoGrind

**Category:** GRD Saturation  
**Role:** Mid/side grit enhancer with stereo spread, drive, and blend.

## Signal Flow

`Input trim → Mid/side split → Mid grit drive + side grind → Recombine → Mix/output trim`

## Parameters
| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `input_trim`  | Input Trim   | Gain staging into the mid/side chain (-18 to +18 dB). |
| `grit`        | Grit         | Amount of mid-channel saturation. |
| `stereo`      | Stereo       | Side-channel emphasis (0–1.5x). |
| `drive`       | Drive        | Extra gain into the grit stages. |
| `mix`         | Mix          | Dry/wet crossfade. |
| `output_trim` | Output Trim  | Final level trim (-18 to +18 dB). |
| `ui_bypass`   | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Push Stereo above 1.0 for wider guitars; keep mix <0.7 to remain mono-safe.
- Combine low grit with high drive to add excitement without overwhelming the mids.
- Goodluck cockpit: grit/stereo macros on top, trims in the footer row.
- Presets: **Wide Grind**, **Mono Punch**, **Air Crush**.
