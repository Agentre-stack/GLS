# Shift Prime — PIT.ShiftPrime

**Category:** Pitch/Time  
**Role:** Precision mono/stereo pitch shifter with formant control.

## Signal Flow

`Input -> Pitch shift core -> Formant/tone control -> Filters -> Mix -> Output`

## Parameters

| ID          | Display Name | Notes |
|-------------|--------------|-------|
| `semitones` | Semitones    | Coarse pitch shift. |
| `cents`     | Cents        | Fine pitch shift. |
| `formant`   | Formant      | Formant/tone compensation. |
| `hpf`       | HPF          | Removes low noise. |
| `lpf`       | LPF          | Tames highs. |
| `mode`      | Mode         | Clean vs Dirty (adds drive). |
| `mix`       | Mix          | Dry/wet. |

## Usage Notes
- Clean mode shines on vocals/harmonies; Dirty for creative FX.
- Combine small +/- cents for doubling, or ±12 for harmonies.

## Known Limitations
- Pitch engine currently placeholder; replace with phase vocoder/PSOLA.
- Needs latency reporting once DSP finalized.

## Phase 2/3 TODOs
- [ ] Integrate high-quality pitch shifting (elastique-style).
- [ ] Add latency compensation + correlation meter.
- [ ] Provide vocal/guitar preset bank.
