# Growl Warp — PIT.GrowlWarp

**Category:** Pitch/Time  
**Role:** Downward pitch + formant warp and grit generator.

## Signal Flow

`Input -> Pitch down core -> Formant warp -> Drive -> Output`

## Parameters

| ID               | Display Name | Notes |
|------------------|--------------|-------|
| `semitones_down` | Semitones    | How far to pitch down (0 to -24). |
| `growl`          | Growl        | Emphasizes low-mid growl. |
| `formant`        | Formant      | Adjusts formant resonance. |
| `drive`          | Drive        | Distortion amount. |
| `mix`            | Mix          | Dry/wet blend. |

## Usage Notes
- Use small growl amounts for subtle bass thickening, large for monstrous FX.
- Combine with burst mode (future) for rhythmic formant sweeps.

## Known Limitations
- Pitch engine is placeholder; needs granular or phase-vocoder approach.
- Drive currently a simple tanh; add multi-stage waveshaper next.

## Phase 2/3 TODOs
- [ ] Implement proper pitch shifting + latency reporting.
- [ ] Add burst/sync options and envelope follower.
- [ ] Create cinematic bass presets.
