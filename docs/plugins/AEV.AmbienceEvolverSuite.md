# Ambience Evolver Suite — AEV.AmbienceEvolverSuite

**Category:** Restoration/Spatial  
**Role:** Hybrid ambience remover/noise profiler for post/dialogue polishing, presented in the Goodluck cockpit (logo header, hero display with meters, footer trims + soft bypass). Now supports three profile slots with per-slot stereo captures.

## Signal Flow

`Input Trim -> Ambience Estimator -> Noise Profile Gate (per-slot capture) -> Transient Guard -> Tone Match/HF Recover -> Mix -> Output Trim`

The Phase‑3 UI provides a teal RMS bar + capture-progress meter in the center display, macro ambience controls on the left, tone recovery on the right, and footer sliders for `Input`, `Ambience Mix`, `Output`, and `Soft Bypass`.

## Parameters (Phase 2)

| ID                 | Display Name | Range / Units | Notes |
|--------------------|--------------|---------------|-------|
| `ambience_level`   | Ambience     | 0…1           | Amount of ambience energy to subtract. |
| `deverb`           | De-Verb      | 0…1           | Smear subtraction depth (higher = drier). |
| `noise_suppression`| Noise        | 0…1           | Blend of adaptive gate vs. broadband suppression. |
| `transient_protect`| Transient    | 0…1           | Preserves onsets during heavy ambience removal. |
| `tone_match`       | Tone Match   | 0…1           | Blends cleaned signal toward dry tonal reference. |
| `hf_recover`       | HF Recover   | 0…1           | Restores air lost during noise removal (≈ +6 dB max). |
| `mix`              | Ambience Mix | 0…100 %       | Blend between the cleaned signal and the original (post input trim). |
| `input_trim`       | Input Trim   | -24…+24 dB    | Pre-processing gain staging for hot/noisy sources. |
| `output_trim`      | Output Trim  | -12…+12 dB    | Final level control. |
| `ui_bypass`        | Soft Bypass  | On/Off        | Cockpit bypass that keeps latency and capture buffers intact. |
| `profile_slot`     | Profile Slot | Slot 1/2/3    | Selects which capture slot to read/write. |

## Usage Notes
- Hit “Capture Profile” during room tone to lock a noise reference per slot; the hero display shows progress %, RMS, and the current noise floor in dB for the selected slot.
- Start with Ambience 0.4–0.5, De-Verb 0.35 for most dialogue. Increase Transient Protect when plosives smear.
- Use Ambience Mix for quick “half processed” blends if the fully dry result feels lifeless.
- HF Recover compensates top-end after aggressive noise suppression—keep under 0.7 for natural tone.

## Known Limitations
- Profile capture is stored per slot, but mid/side weighting and multi-length captures remain future work.

## Phase 2/3 TODOs
- [x] Add LUFS/RMS meter and profile status indicator.
- [x] Store/recall multiple room profiles per session (3 slots).
