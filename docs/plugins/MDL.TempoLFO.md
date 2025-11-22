# Tempo LFO — MDL.TempoLFO

**Category:** Mod/Delay/LFO  
**Role:** Tempo-synced amplitude LFO utility with smoothing and shape morphing.

## Signal Flow

`Input -> Tempo-synced modulation gain -> Output`

## Parameters
- `depth`: Modulation depth applied to the gain curve.
- `offset`: DC offset for moving the modulation above/below unity.
- `smoothing`: Slews the LFO for softer transitions.
- `shape`: Select sine, triangle, or square waveform.
- `sync`: Host-synced divisions (1/1 .. 1/8).

## Usage Notes
- Offset around −0.2 with depth 0.5 creates gentle ducking synced to the host transport.
- Increase smoothing when using square mode to avoid zipper artifacts on percussive buses.
