# Trans Tube X — GRD.TransTubeX

**Category:** Saturation  
**Role:** Transient-aware tube shaper that punches attacks while saturating sustain.

## Signal Flow

`Input trim → Transient tracker → Dynamic drive block → Tube shaper → Tone filter → Mix/output trim`

## Parameters

| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `input_trim`  | Input Trim   | Gain staging into the shaper (-18 to +18 dB). |
| `drive`       | Drive        | Global drive into the tube shaper. |
| `trans_sens`  | Trans Sens   | Sensitivity of the transient tracker (0–1). |
| `attack_bias` | Attack Bias  | How much transient energy hits the front vs the tail. |
| `tone`        | Tone         | 500 Hz–12 kHz low-pass on the wet path. |
| `mix`         | Mix          | Dry/wet balance. |
| `output_trim` | Output Trim  | Final level match (-18 to +18 dB). |
| `ui_bypass`   | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Higher `Trans Sens` + lower `Attack Bias` keeps the punch while saturating sustain.
- Works well as a limiter-alternative on drum buses with Mix < 0.75.
- Goodluck cockpit with transient/drive macros on top and trims in the footer row.
- Presets: **Punch Tube**, **Sustain Glue**, **Bright Crush**.

## Known Limitations
- Tracker is instantaneous per-sample; overshoot may occur on smeared transients.

## Phase 2/3 TODOs
- [ ] Offer look-ahead or smoothing options for the transient tracker.
- [ ] Mode switch for multi-band operation.
