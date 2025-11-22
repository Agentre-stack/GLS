# Air Glass — EQ.AirGlass

**Category:** EQ / Tone shaping  
**Role:** Add airy sheen while managing harshness around the top octave. Includes factory presets.

## Signal Flow

`Input → High-shelf enhancer → Harmonic saturator blend → Dynamic de-harsh band → Output trim`

## Parameters

| ID               | Display Name    | Notes |
|------------------|-----------------|-------|
| `air_freq`       | Air Freq        | Sets the tilt frequency for the shelf (6–20 kHz). |
| `air_gain`       | Air Gain        | Boost/cut of the airy shelf (-6 to +12 dB). |
| `harmonic_blend` | Harmonic Blend  | Crossfade between clean shelf and soft-sat harmonic layer. |
| `deharsh`        | DeHarsh         | Controls the dynamic notch depth against harsh bands. |
| `output_trim`    | Output Trim     | Final gain staging (-12 to +12 dB). |
| `ui_bypass`      | Soft Bypass     | Goodluck footer bypass. |

## Usage Notes
- Start with modest Air Gain (+2 to +4 dB) and raise slowly; the harmonic blend can add gloss to vocals or synths.
- Use DeHarsh when cymbals or vocals get brittle—values around 0.4–0.6 tame resonant spikes without dulling.
- Output trim helps maintain level-matched comparisons when boosting highs aggressively.
- Presets: **Pop Vocal Air** (bright + deharsh), **Master Shimmer** (gentle mastering lift), **Cymbal Brighten** (aggressive top with more deharsh).

## Known Limitations
- Harsh-band detector is single-band; future work should add multi-band or frequency tracking.
- No oversampling yet, so extremely bright boosts above +10 dB can alias on dense material.

## Phase 2/3 TODOs
- [ ] Offer alternate high-shelf curves (API-style, Maag-style).
- [ ] Add optional oversampling + auto-gain.
- [x] Provide genre-specific presets (pop vocal, mastering air, cymbal brite).
