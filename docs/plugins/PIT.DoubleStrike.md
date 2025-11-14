# Double Strike — PIT.DoubleStrike

**Category:** Pitch/Time  
**Role:** Dual-voice shifter for harmonies and thickening.

## Signal Flow

`Input -> Dual pitch engines -> Filters -> Pan/Spread -> Mix -> Output`

## Parameters

| ID              | Display Name | Notes |
|-----------------|--------------|-------|
| `voice_a_pitch` | Voice A Pitch| Coarse semitone shift. |
| `voice_b_pitch` | Voice B Pitch| Coarse semitone shift. |
| `detune`        | Detune       | Fine offset between voices. |
| `spread`        | Spread       | Stereo width (-1..1). |
| `hpf`           | HPF          | Cleans lows. |
| `lpf`           | LPF          | Softens highs. |
| `mix`           | Mix          | Dry/wet. |

## Usage Notes
- Use complementary intervals (+7 / -5) for instant double-tracking.
- Dial Spread down for mono compatibility; up for wide vocals.

## Known Limitations
- Pitch DSP is placeholder; need independent modulation + delay.
- Monitoring/metering not yet implemented.

## Phase 2/3 TODOs
- [ ] Replace placeholder pitch shift with elastique-style engine.
- [ ] Add per-voice delay + modulation depth.
- [ ] Provide harmony presets.
