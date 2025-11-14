# Shimmer Fall — PIT.ShimmerFall

**Category:** Pitch/Time  
**Role:** Shimmer reverb feedback architecture for atmospheric tails.

## Signal Flow

`Input -> Reverb core -> Pitch shift in feedback loop -> Output`

## Parameters

| ID            | Display Name | Notes |
|---------------|--------------|-------|
| `pitch_interval` | Pitch Interval | Shift inserted in feedback path. |
| `feedback`    | Feedback      | Overall regeneration. |
| `damping`     | Damping       | High-frequency damping amount. |
| `time`        | Time          | Base delay time (0.05–4 s). |
| `mix`         | Mix           | Dry/wet. |

## Usage Notes
- Use pitch intervals between +7 and +12 for classic shimmer; keep feedback under 0.8 to avoid runaway.
- Damping keeps repeats from building harshness; increase for smoother pads.

## Known Limitations
- Pitch shifting is currently a placeholder multiplier; integrate true granular shifter next.
- No diffusion/reverb tank yet.

## Phase 2/3 TODOs
- [ ] Replace pseudo shift with real algorithm.
- [ ] Add modulation + diffusion stages.
- [ ] Provide pad/guitar preset bank.
