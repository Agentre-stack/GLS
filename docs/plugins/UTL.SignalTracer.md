# Signal Tracer — UTL.SignalTracer

**Category:** Utility/Analysis  
**Role:** Visual signal path, polarity, and level inspector.

## Signal Flow

`Input -> Tap buffer(s) -> Visualization-only outputs`

## Parameters

| ID             | Display Name | Notes |
|----------------|--------------|-------|
| `tap_select`   | Tap Select   | Choose which internal tap to monitor. |
| `phase_view`   | Phase View   | Toggle Lissajous/phase scope. |
| `peak_hold`    | Peak Hold    | Hold time for peak meters. |
| `rms_window`   | RMS Window   | RMS integration window. |
| `routing_mode` | Routing Mode | Stereo / Mid / Side / Mono views. |

## Usage Notes
- Drop anywhere in the chain to visualize polarity and level.
- Use Mid/Side routing to inspect stereo balance quickly.

## Known Limitations
- GUI scope not implemented yet; current build is data-only.
- No presets or tap naming; add once UI exists.

## Phase 2/3 TODOs
- [ ] Implement oscilloscope + correlation meter.
- [ ] Add tap labeling + recall.
- [ ] Provide presets for common tracing setups.
