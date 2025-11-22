# Wavesmear — GRD.WavesmearDistortion

**Category:** Distortion  
**Role:** Smears transients into a saturator for synthy, smeared drive textures.

## Signal Flow

`Input -> High-pass pre-filter -> Smear integrator -> Drive/tanh -> Tone low-pass -> Mix -> Output trims`

## Cockpit (Phase 3)
- Goodluck header/footer with accent, macro rotaries for Smear/Drive/Mix, and focused Pre Filter + Tone row.
- Input/Output trims and Soft Bypass sit on the right so level staging is always visible.
- Preset bank lives in the host program list.

## Parameters

| ID             | Display Name | Notes |
|----------------|--------------|-------|
| `pre_filter`   | Pre Filter   | High-pass before smear (60–5000 Hz). |
| `smear_amount` | Smear        | Blends between dry and previous-sample memory. |
| `drive`        | Drive        | Input gain into saturation. |
| `tone`         | Tone         | 800–12000 Hz low-pass after distortion. |
| `mix`          | Mix          | Dry/wet. |
| `input_trim`   | Input Trim   | -18 to +18 dB before processing. |
| `output_trim`  | Output Trim  | -18 to +18 dB after processing. |
| `ui_bypass`    | Soft Bypass  | Post-trim mute of the processing path. |

## Factory Presets
- Smear Lead — tight high-pass, moderate smear/drive, bright tone.
- Drone Wash — lower drive with heavier smear for ambient wash.
- Bass Sputter — lowest filter, hotter drive with darker tone to keep lows intact.

## Usage Notes
- High Smear plus moderate Drive yields pad-like sustain; lower smear keeps transient bite.
- Keep Pre Filter higher (>300 Hz) on bass to avoid muddying the smear loop.
- Use Output Trim to regain headroom after heavy smear/drive settings.

## Known Limitations
- Smear loop is first-order; complex diffusions and oversampling are still backlog candidates.
