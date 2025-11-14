# Chopper Trem — MDL.ChopperTrem

**Category:** Mod/Delay/LFO  
**Role:** Rhythmic tremolo / gate sequencer with tempo sync.

## Signal Flow

`Input -> Sequenced amplitude mod core -> Output`

## Parameters

| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `depth`       | Depth        | Tremolo depth. |
| `rate_sync`   | Rate Sync    | Tempo divisions. |
| `pattern`     | Pattern      | Step pattern selection. |
| `smooth`      | Smoothing    | Interpolates steps. |
| `hpf`         | HPF          | Keeps lows clean. |
| `mix`         | Mix          | Dry/wet blend. |

## Usage Notes
- Great for chopped pads or gating guitars—sync to dotted 8ths or 16ths.
- Increase Smooth for more musical trem vs. hard chop.

## Known Limitations
- Pattern engine is placeholder; need UI for drawing sequences.
- No host sync tests yet—validate BPM locking next phase.

## Phase 2/3 TODOs
- [ ] Add pattern editor and preset patterns.
- [ ] Implement band-split for low-retention.
- [ ] Style UI with BPM indicators.
