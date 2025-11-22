# Warm Lift — GRD.WarmLift

**Category:** GRD Saturation  
**Role:** Bus sweetener that adds warmth/shine with drive, tighten, and mix controls.

## Signal Flow

`Input trim → Tighten HPF → Warmth low shelf → Shine high shelf → Drive → Mix/output trim`

## Parameters
| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `input_trim`  | Input Trim   | Gain staging into the tone stack (-18 to +18 dB). |
| `warmth`      | Warmth       | Boost/cut low shelf (±12 dB). |
| `shine`       | Shine        | Boost/cut high shelf (±12 dB). |
| `drive`       | Drive        | Saturation amount. |
| `tighten`     | Tighten      | High-pass frequency to clean lows (20–220 Hz). |
| `mix`         | Mix          | Parallel blend. |
| `output_trim` | Output Trim  | Final gain staging (-18 to +18 dB). |
| `ui_bypass`   | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Set Warmth around +3 dB and Tighten near 80 Hz to thicken mixes without mud.
- Use Mix 0.6–0.8 for parallel tone lifting; push Shine for modern lift.
- Goodluck cockpit with warmth/shine macros on top and trims in the footer row.
- Presets: **Vocal Warm Lift**, **Guitar Glow**, **Bus Glue**.
