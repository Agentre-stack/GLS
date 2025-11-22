# Vocal Presence Comp — DYN.VocalPresenceComp

**Category:** Dynamics / Vocal coloration  
**Role:** Presence‑focused compressor with optional air shelf boost.

## Signal Flow

`Input → Presence band compressor → Air shelf → Mix`

## Parameters

| ID | Display Name | Notes |
|----|--------------|-------|
| `presence_thresh` | Presence Thresh | Threshold for the presence band. |
| `presence_range` | Presence Range | Amount of emphasis/cut applied. |
| `presence_freq` / `presence_q` | Presence Freq/Q | Center frequency and Q. |
| `air_gain` | Air Gain | High‑shelf boost/cut. |
| `mix` | Mix | Parallel blend. |

## Usage Notes
- Use positive Range to lift articulation after the threshold, negative to smooth harshness.
- Air Gain lets you add sheen once the presence band is controlled.

## Known Limitations
- Presence compressor uses fixed attack/release; future update should expose timing controls.

## Phase 2/3 TODOs
- [ ] Add visual feedback of presence gain reduction.
- [ ] Provide preset voicings (male/female, airy/solid).
