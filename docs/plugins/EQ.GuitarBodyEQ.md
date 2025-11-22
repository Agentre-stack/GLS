# Guitar Body EQ — EQ.GuitarBodyEQ

**Category:** EQ / Instrument voicing  
**Role:** Shape acoustic/electric guitar fundamentals, tame mud, boost pick attack, and lift air.

## Signal Flow
`Input → Body bell → Mud notch → Pick shelf → Air shelf → Output`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `body_freq` | Body Freq | Center frequency of the resonance bell (80–500 Hz).
| `body_gain` | Body Gain | Amount of body enhancement/cut (-12 to +12 dB).
| `mud_cut` | Mud Cut | Frequency of the notch to scoop buildup (80–400 Hz).
| `pick_attack` | Pick Attack | High-shelf boost/cut around 2.5 kHz for articulation.
| `air_lift` | Air Lift | High-shelf boost/cut around 8 kHz for sparkle.

## Usage Notes
- For strummed acoustics, raise Body Gain ~+3 dB at 120–180 Hz and trim Mud Cut near 200 Hz for clarity.
- Use Pick Attack to highlight fingerstyle detail or reduce harsh picking noises.
- Air Lift adds shimmer to DI electric cleans; keep modest to avoid hiss.

## Known Limitations
- Shelves use fixed Q and fixed pivot frequencies (2.5/8 kHz).
- No input/output trims—stack with Channel Pilot for gain staging.

## Phase 2/3 TODOs
- [ ] Add output trim + auto gain.
- [ ] Offer selectable shelf pivot points.
- [ ] Provide acoustic/electric preset banks.
