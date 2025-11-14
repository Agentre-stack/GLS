# Channel Strip One — GLS.ChannelStripOne

**Category:** Core Mix/Bus  
**Role:** Gate + compressor + 4-band EQ + saturation for every tracking channel.

## Signal Flow

`Input -> Gate -> Compressor -> 4-band EQ -> Saturation -> Output`

## Parameters (Phase 1)

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
| `mix`          | Mix          | 0..1          | Dry/wet for saturation block. |

## Usage Notes
- Start with gate thresholds around -45 dB for drum close mics.
- Compressor defaults aim for 6–8 dB GR on vocals; tweak ratio/attack per source.
- Use shelves for quick tone shaping; keep saturation low for subtle glue.

## Known Limitations
- DSP placeholders: compression + saturation need more realism.
- UI is generic; parameter grouping TBD.

## Phase 2/3 TODOs
- [ ] Improve gate detector (hysteresis) and compressor knee.
- [ ] Add per-band frequency controls and metering.
- [ ] Supply tracking presets (vocals, guitars, drums).
