# Top Fizz — GRD.TopFizz

**Category:** Harmonic Enhancer  
**Role:** Adds controlled high-end fizz/brightness with odd/even blending.

## Signal Flow

`Input trim → High-pass band split → Harmonic generator → Smoothing LPF → Mix/output trim`

## Parameters

| ID              | Display Name | Notes |
|-----------------|--------------|-------|
| `input_trim`    | Input Trim   | Gain staging into the fizz band (-18 to +18 dB). |
| `freq`          | Freq         | Corner frequency for the exciter band (2–16 kHz). |
| `amount`        | Amount       | Drive into the harmonic generator. |
| `odd_even_blend`| Odd/Even     | Crossfade between odd (tanh) and even-rich curves. |
| `deharsh`       | DeHarsh      | Sets the smoothing LPF cutoff to tame harshness. |
| `mix`           | Mix          | Dry/wet. |
| `output_trim`   | Output Trim  | Level-match after fizz (-18 to +18 dB). |
| `ui_bypass`     | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Keep `freq` around 6–10 kHz for vocals; drop lower for guitars.
- Use DeHarsh >0.5 when driving the exciter hard to keep transients smooth.
- Goodluck cockpit with fizz macros on top and trims in the footer row.
- Presets: **Vocal Air Fizz**, **Bright Guitar**, **Master Sparkle**.

## Known Limitations
- No oversampling yet (aliasing may appear at extreme drive/amount).

## Phase 2/3 TODOs
- [ ] Optional oversampling for harsher settings.
- [ ] Combine with shelving EQ macro for quick mastering tweaks.
