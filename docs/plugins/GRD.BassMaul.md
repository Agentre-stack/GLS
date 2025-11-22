# Bass Maul — GRD.BassMaul

**Category:** GRD Saturation  
**Role:** Low-end sculpting saturator with tightness filtering, sub boost, and blend control.

## Signal Flow

`Input -> Tight high-pass -> Waveshaper drive -> Sub low-pass lift -> Blend/trim -> Output`

## Parameters
- `drive`: Amount of saturation applied to the tight-band signal.
- `sub_boost`: Gain applied to the sub low-pass component (dB).
- `tightness`: High-pass cutoff that keeps low-mids focused.
- `blend`: Dry/Wet mix between the original and processed signal (mirrored in footer slider).
- `output_trim`: Final level trim (dB).
- `input_trim`: Pre-drive gain for staging into the HPF/shaper.
- `ui_bypass`: Soft bypass toggle (cockpit footer) separate from the DAW bypass.

## Usage Notes
- Use Tightness around 80–120 Hz to keep mud out before hitting Drive.
- Combine moderate drive (0.6) with +6 dB Sub Boost for modern bass buss “maul” weight without overloading subs.
- Input trim lets you slam the shaper harder without nudging track gain; Output Trim in the right column + footer keeps loudness honest.
- Soft Bypass is latency-safe; use it to audition Goodluck’s new cockpit UI before deferring to the host bypass.

## Phase 3 UI Snapshot

- **Macro column:** Drive + Tightness are the big amber knobs, matching the suite-wide grid.
- **Right column:** Sub Boost and Output Trim in the micro stack, both sharing the GRD accent with new labels.
- **Center display:** Magenta transfer curve + sub energy block visualize drive/tightness/sub-lift in real time.
- **Footer:** Input Trim → Dry/Wet slider → Soft Bypass complete the Goodluck flight-deck layout.
