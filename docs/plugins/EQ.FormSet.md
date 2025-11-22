# Form Set — EQ.FormSet

**Category:** EQ / Formant design  
**Role:** Animate synthetic vowel/formant resonances with controllable width, movement, and blend.

## Signal Flow
`Input → Formant band-pass network → Modulated movement → Intensity gain scaling → Wet/dry mix`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `input_trim` | Input Trim | Gain into the formant network (-18 to +18 dB).
| `formant_freq` | Formant Freq | Base center frequency (200 Hz – 4 kHz).
| `formant_width` | Formant Width | Resonance width multiplier.
| `movement` | Movement | Adds LFO motion to the formant center.
| `intensity` | Intensity | Boost amount applied to the moving resonance.
| `mix` | Mix | Dry/wet blend.
| `output_trim` | Output Trim | Level-match after formant boost (-18 to +18 dB).
| `ui_bypass` | Soft Bypass | Goodluck footer bypass (logo/footer).

## Usage Notes
- Position Formant Freq around 700–900 Hz for classic “ah/oh” tones, or higher for nasal textures.
- Movement + Intensity create talking/singing effects—start with modest movement (0.2–0.4) to avoid seasick shifts.
- Mix lets you tuck the effect under the original signal for realistic enhancement; trims/bypass live in the Goodluck footer.
- Presets: **Vocal Morph**, **Guitar Talk**, **FX Drone**.

## Known Limitations
- Single formant per channel; future updates should support multiple stacked peaks.
- No sync option for the movement LFO.

## Phase 2/3 TODOs
- [ ] Add tempo-sync + different modulation shapes.
- [ ] Support multiple formant slots with per-slot mix.
- [ ] Provide preset macros (Vowel morphs, Guitar vocalize, FX drones).
