# Parallel Press — GLS.ParallelPress

**Category:** Core Mix/Bus  
**Role:** Dual-filtered parallel compressor with drive, wet EQ, and blend trim presented in the Goodluck cockpit.

## Signal Flow

`Input Trim -> Wet HPF -> Wet LPF -> Envelope Follower -> Gain Computer -> Drive -> Wet/Dry Trims -> Auto Makeup -> Dry/Wet Mix -> Output Trim`

The Phase‑3 UI now uses the shared Goodluck rack: header with the GLS logo/title, teal hero panel with GR meter + filter passband view, macro knobs on the left, wet-path EQ trims on the right, and footer trims (`Input`, `Dry/Wet`, `Output`, `Soft Bypass`).

## Parameters (Phase 2)

| ID             | Display Name | Range / Units | Notes |
|----------------|--------------|---------------|-------|
| `drive`        | Drive        | 0…1 (unitless)| Saturation amount applied after compression. |
| `comp_thresh`  | Comp Thresh  | -48…0 dB      | Threshold for the RMS envelope detector. |
| `comp_ratio`   | Comp Ratio   | 1:1…20:1      | Parallel compression ratio. |
| `attack`       | Attack       | 0.1…50 ms     | Detector attack time. |
| `release`      | Release      | 10…500 ms     | Detector release time. |
| `hpf_to_wet`   | HPF to Wet   | 20…400 Hz     | Removes mud before the wet path hits the drive stage. |
| `lpf_to_wet`   | LPF to Wet   | 2 kHz…20 kHz  | Tames fizz in the wet path. |
| `wet_level`    | Wet Level    | -24…+6 dB     | Gain for the compressed/processed path. |
| `dry_level`    | Dry Level    | -24…+6 dB     | Gain for the dry reference signal. |
| `mix`          | Dry / Wet    | 0…100 %       | Final blend between the processed stack and original dry input. |
| `input_trim`   | Input        | -24…+24 dB    | Goodluck footer input trim before the compression stage. |
| `output_trim`  | Output       | -24…+24 dB    | Output trim at the footer for level matching. |
| `auto_gain`    | Auto Gain    | Off / On      | Applies makeup gain proportional to the observed GR (teal meter). |
| `ui_bypass`    | Soft Bypass  | Off / On      | Cockpit bypass that keeps latency/state intact for comparisons. |

## Usage Notes
- Start with wet/dry at 0 dB, then trim dry to taste for drum buses needing weight.
- HPF/LPF let you drive mids without pumping the subs or hissy highs.
- Attack/release default to punchy bus settings; lengthen release for vocals or strings.
- The teal GR meter shows up to 30 dB of reduction. Auto Gain works off the same reading and can be toggled in the footer.
- The Dry/Wet footer slider is great for quick "parallel vs. original" checks while keeping the Wet/Dry level trims for detailed balancing.

## Known Limitations
- Knee remains fixed—future Phase‑3+ work could expose it.

## Phase 2/3 TODOs
- [x] Add GR meter + auto gain trimming.
- [x] Add preset bank (Drum Crush, Vocal Glue, Bus Lift).
