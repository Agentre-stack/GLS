# Micro Shift — PIT.MicroShift

**Category:** Pitch/Time  
**Role:** Dual detune + delay widener for instant stereo spread.

## Signal Flow

`Input -> Dual micro chorus lines -> HPF -> Stereo width -> Mix -> Output`

## Parameters

| ID         | Display Name | Notes |
|------------|--------------|-------|
| `detune_l` | Detune L     | LFO rate/depth for left voice (−20 to +20 cents feel). |
| `detune_r` | Detune R     | Same for right voice. Opposing signs give extra spread. |
| `delay_l`  | Delay L (ms) | Static pre-delay before the left engine (0–30 ms). |
| `delay_r`  | Delay R (ms) | Static pre-delay for the right engine. |
| `width`    | Width        | Converts wet signal to mid/side and scales the side component. |
| `hpf`      | HPF          | 12 dB/oct high-pass on wet signal (20–1000 Hz). |
| `mix`      | Mix          | Global dry/wet. |

## Usage Notes
- Keep detunes asymmetric (e.g. −6/+6) with staggered delays (8/12 ms) for the classic microshift feel.
- Use `width < 1` when folding to mono to avoid phasey cancellations.
- HPF removes rumble from the doubled path so lows stay anchored.

## Known Limitations
- Chorus-based pitch approximation; replace with granular/pitch-shifter core for full-quality version.
- No tempo-sync for delay offsets yet.

## Phase 2/3 TODOs
- [ ] Upgrade detune voices to elastique-quality pitch shifting.
- [ ] Add global modulation rate control exposed to automation.
- [ ] Provide preset bank for vocals, guitars, synths.
