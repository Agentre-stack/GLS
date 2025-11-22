# Latency Lab — UTL.LatencyLab

**Category:** Utility / Analysis  
**Role:** Delay-line calibration lab with ping generator, mix trims, and the Goodluck latency cockpit (logo header, hero timeline visual, footer trims/dry-wet/bypass).

## Signal Flow
`Input Trim → Ping injection (optional) → Global Delay Line → Mix Crossfade → Output Trim`

## Parameters
| ID | Display Name | Notes |
|----|--------------|-------|
| `latency_ms`    | Latency (ms) | Main delay time (0–500 ms) applied equally to every channel. |
| `ping_enable`   | Ping | Enables the calibration ping generator. |
| `ping_interval` | Ping Interval | Interval between pings (100–4000 ms). |
| `ping_level`    | Ping Level | Level of the injected ping (-48…0 dB). |
| `mix`           | Mix | Crossfade between delayed and dry signal for parallel alignments. |
| `input_trim`    | Input Trim | Pre-delay gain staging (-24…+24 dB). |
| `output_trim`   | Output Trim | Post chain trim (-24…+24 dB). |
| `ui_bypass`     | Soft Bypass | Cockpit bypass for instant A/B without touching the host enable. |

## Usage Notes
- Use the ping generator to fire single-sample clicks at known intervals; the hero display highlights latency vs. ping so you can line up DAW delay compensation or hardware inserts quickly.
- Keep Mix below 100% when you want to compare delayed vs. unprocessed signals; at 100% the plugin becomes a straight delay line.
- Input/Output trims are in the footer so you can level-match while compensating for analog insert loss; Soft Bypass keeps the delay state intact while you check alignment.

## Known Limitations
- Ping is mono-summed; stereo independent pings are not implemented yet.

## Phase 2/3 TODOs
- [x] Cockpit UI with latency timeline, ping controls, and footer trims/mix/bypass.
- [x] Ping generator + Goodluck mix path.
- [ ] Optional presets for common round-trip figures (e.g., “Apollo 32 Console”, “Hybrid Sum Bus”) once the preset sprint begins.
