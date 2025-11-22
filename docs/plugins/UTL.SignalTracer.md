# Signal Tracer — UTL.SignalTracer

**Category:** Utility/Analysis  
**Role:** Visual signal path, polarity, and level inspector with Goodluck cockpit (logo header, oscilloscope/correlation hero, tap labeling, footer trims/bypass).

## Signal Flow

`Input Trim -> Tap buffer(s) -> Scope/Correlation Visuals -> (Optional routing/solo) -> Output Trim`

## Parameters

| ID             | Display Name | Notes |
|----------------|--------------|-------|
| `tap_select`   | Tap Select   | Choose which internal tap (Input/Pre/Post/Side) to monitor. |
| `phase_view`   | Phase View   | Switch between Lissajous, Correlation bar, and Vectorscope renderings. |
| `peak_hold`    | Peak Hold    | Hold time for peak meters. |
| `rms_window`   | RMS Window   | RMS integration window. |
| `routing_mode` | Routing Mode | Stereo / Mid / Side / Solo Tap routing for the analyzer output. |
| `input_trim`   | Input Trim   | Gain staging before the analyzer (-24…+24 dB). |
| `output_trim`  | Output Trim  | Final output trim. |
| `ui_bypass`    | Soft Bypass  | Disables the tracer while preserving latency/state. |

## Usage Notes
- Drop anywhere in the chain to visualize polarity and level; the teal scope shows the selected tap along with RMS/peak meters and a phase-correlation bar.
- Tap labels can be edited directly in the cockpit and saved into three preset slots for quick recall (e.g. “Kick Bus”, “Snare Bus”, “FX Return”).
- Use Mid/Side routing to inspect stereo balance quickly; the Solo Tap mode lets you route a specific tap to the output for forensic listening.
- Input/Output trims sit in the footer for easy level matching.

## Known Limitations
- Additional preset packs can be authored later; current UI provides three user slots but no factory banks yet.

## Phase 2/3 TODOs
- [x] Implement oscilloscope + correlation meter using stored tap metrics.
- [x] Add tap labeling + recall slots.
- [ ] Provide curated presets for common tracing setups.
