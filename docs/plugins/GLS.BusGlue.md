# Bus Glue — GLS.BusGlue

**Category:** Core Mix/Bus  
**Role:** Console-style glue compressor with sidechain HPF and wet/dry mix.

## Signal Flow

`Input -> Detector (SC HPF + RMS) -> Gain Computer -> Gain Smoothing -> Mix -> Output Trim`

## Parameters (Phase 2)

| ID      | Display Name | Range / Units | Notes |
|---------|--------------|---------------|-------|
| `thresh`| Threshold    | -48…0 dB      | Bus compression threshold. |
| `ratio` | Ratio        | 1…20:1        | Compression ratio. |
| `attack`| Attack       | 0.1…200 ms    | Detector attack. |
| `release`| Release     | 5…1000 ms     | Recovery time. |
| `knee`  | Knee         | 0…18 dB       | Soft knee width. |
| `sc_hpf`| SC HPF       | 20…400 Hz     | Sidechain high-pass frequency. |
| `input_trim`| Input Trim | -24…+24 dB  | Gain staging into the detector (footer control). |
| `mix`   | Dry/Wet      | 0…1           | Parallel blend (footer control). |
| `output`| Output       | -18…+18 dB    | Post-compression trim (footer control). |
| `ui_bypass`| Soft Bypass | On/Off      | Latency-safe bypass on the footer strip. |

## Usage Notes
- Start with Threshold -18 dB, Ratio 4:1, Attack 10 ms, Release 100 ms for drum buses.
- Engage SC HPF around 80 Hz to keep kicks from over-triggering compression.
- Use Mix < 1 for parallel glue without losing transients.

## Phase 3 UI Snapshot
- Goodluck cockpit: logo header, macro knobs (threshold/ratio/attack/release) on the left, cyan GR meter + stats in the center, SC/attack utilities on the right.
- Footer houses Input Trim → Dry/Wet → Output Trim → Soft Bypass for instant A/B.
- Latest build includes a teal gain-reduction meter strip (BusGlueVisual) for at-a-glance compression depth.

## Known Limitations
- Single SC filter shared across channels (per design).
- Auto makeup still on the TODO list; presets included.

## Phase 2/3 TODOs
- [ ] Auto makeup option tied to detected GR.
- [x] Preset set (Drum Glue, MixBus Glue, Vocal Bus).
