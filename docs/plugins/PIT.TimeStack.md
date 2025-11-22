# Time Stack — PIT.TimeStack

**Category:** Pitch/Time  
**Role:** Four-tap tempo-free delay designer with per-tap pan/level controls.

## Signal Flow

`Input -> Mono sum -> 4 delay taps -> HPF -> LPF -> Mix -> Output`

## Parameters

| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `tapN_time`   | Tap Time N   | 10–2000 ms base delay with swing offsets. |
| `tapN_level`  | Tap Level N  | Linear gain per tap (0–1). |
| `tapN_pan`    | Tap Pan N    | Constant-power pan left (−1) to right (+1). |
| `hpf`         | HPF          | 12 dB/oct high-pass on wet path (20–2 kHz). |
| `lpf`         | LPF          | 12 dB/oct low-pass on wet path (2–20 kHz). |
| `swing`       | Swing        | Offsets odd/even tap times for rhythmic push/pull. |
| `mix`         | Mix          | Dry/wet balance. |

## Usage Notes
- Tap times are absolute for now—use swing to fake dotted/triplet movement until host-sync ships.
- Tap levels are linear; for more natural decays keep each successive tap −3 to −6 dB lower.
- With stereo inputs the wet output always maintains width even if the host bus is mono.

## Known Limitations
- Delay times are free-running (no host BPM awareness yet).
- Tap engine currently sums mono input; revisit once dual-input routing is required.

## Phase 2/3 TODOs
- [ ] Add host tempo sync + note division selector per tap.
- [ ] Optional diffusion/saturation block before filters.
- [ ] Visual tap overview inside the editor.
