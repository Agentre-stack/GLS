# Channel Strip One — GLS.ChannelStripOne

**Category:** Core Mix/Bus  
**Role:** Gate + compressor + 4-band EQ + saturation for every tracking channel.

## Signal Flow

`Input -> Gate -> Compressor -> 4-band EQ -> Saturation -> Output`

## Parameters (Phase 2)

| ID             | Display Name | Range / Units | Notes |
|----------------|--------------|---------------|-------|
| `gate_thresh`  | Gate Thresh  | dB            | Controls expander threshold. |
| `gate_range`   | Gate Range   | dB            | Amount of attenuation below threshold. |
| `comp_thresh`  | Comp Thresh  | dB            | Main compressor threshold. |
| `comp_ratio`   | Ratio        | 1:1..20:1     | Compression ratio. |
| `comp_attack`  | Attack       | ms            | Attack time. |
| `comp_release` | Release      | ms            | Release time. |
| `low_gain`     | Low Gain     | dB            | Low shelf gain. |
| `low_mid_gain` | Low-Mid Gain | dB            | Low-mid bell. |
| `high_mid_gain`| High-Mid Gain| dB            | High-mid bell. |
| `high_gain`    | High Gain    | dB            | High shelf. |
| `sat_amount`   | Saturation   | %             | Drive amount. |
| `mix`          | Dry/Wet      | 0..1          | Parallel blend (also mirrored in footer). |
| `input_trim`   | Input Trim   | dB            | Pre-chain gain for headroom staging. |
| `output_trim`  | Output Trim  | dB            | Post-chain gain for level match. |
| `ui_bypass`    | Soft Bypass  | 0/1           | Latency-safe soft bypass for the cockpit footer. |

## Usage Notes
- Start with gate thresholds around -45 dB for drum close mics.
- Compressor defaults aim for 6–8 dB GR on vocals; tweak ratio/attack per source.
- Use shelves for quick tone shaping; keep saturation low for subtle glue.
- Input/Output trims live in the footer to A/B loudness quickly; Dry/Wet now sits beside them for parallel comp moves.
- Soft Bypass keeps the phase-3 UI’s macros visible even when you want to audition DAW bypass.

## Phase 3 UI Snapshot

- **Branding:** Goodluck sigil + teal header, preset label on the right, A/B placeholder ready for preset work.
- **Layout:** Four macro knobs (Gate Threshold/Range + Comp Threshold/Ratio) on the left column, tilt meters + EQ bars in the center visual, and micro controls (attack/release/band gains/sat) stacked on the right column.
- **Footer:** Input Trim → Dry/Wet → Output Trim → Soft Bypass, all rendered with the GLS monoline sliders so gain staging happens without diving into other tabs.
- **Validation:** Rebuilt with the new UI and rerun through `vst3validator` (no warnings other than Steinberg’s known IParameterChanges note).

## Known Limitations
- DSP placeholders: compression + saturation need more realism.
- Per-band EQ frequencies are still fixed; phase-3 center scope only visualizes relative gains.

## Phase 2/3 TODOs
- [ ] Improve gate detector (hysteresis) and compressor knee.
- [ ] Add per-band frequency controls and metering overlays.
- [ ] Supply tracking presets (vocals, guitars, drums) plus the preset dropdown plumbing in the header.
