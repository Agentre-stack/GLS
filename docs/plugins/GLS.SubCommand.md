# Sub Command — GLS.SubCommand

**Category:** Core Mix/Bus  
**Role:** Subharmonic generator with controllable crossover, tightness, harmonics, and blend.

## Signal Flow

`Input -> Lowpass Split -> Dynamics (tightness) -> Harmonics/Drive -> Mix w/ Dry -> Output HPF`

## Parameters (Phase 2)

| ID           | Display Name | Range / Units | Notes |
|--------------|--------------|---------------|-------|
| `xover_freq` | Xover Freq   | 40…250 Hz     | Sets the lowpass split between sub and main path. |
| `sub_level`  | Sub Level    | -12…+12 dB    | Makeup gain for the generated sub channel. |
| `tightness`  | Tightness    | 0…1           | Controls sub envelope speed/ducking for punch. |
| `harmonics`  | Harmonics    | 0…1           | Blends in nonlinear saturation for audibility. |
| `out_hpf`    | Out HPF      | 20…120 Hz     | Protects the sum from DC/rumbles post mix. |
| `mix`        | Dry/Wet      | 0…1           | Parallel blend (now lives in the Goodluck footer). |
| `input_trim` | Input Trim   | -24…+24 dB    | Stages level before the low split/drive engine. |
| `output_trim`| Output Trim  | -24…+24 dB    | Footer slider for loudness matching. |
| `ui_bypass`  | Soft Bypass  | On/Off        | Latency-safe bypass toggle in the footer strip. |

## Usage Notes
- Keep crossover near 80–90 Hz for kicks/bass; push lower for cinematic subs.
- Tightness around 0.6 reins in decay while letting transients through.
- Add harmonics when subs need translation on smaller speakers.
- HPF defaults to 35 Hz; raise when printing to streaming-friendly masters.
- Input/Output trims now sit alongside Dry/Wet and Soft Bypass in the footer for quick level matching.
- Cockpit center visual shows crossover + HPF markers plus Sub/Tight/Harmonic bars so you can see where energy is landing.

## Phase 3 UI Snapshot

- Goodluck header with sigil + preset label, macro knobs on the left (Xover/Sub/Tight/Harm).
- Center: Sub energy visual with teal markers for crossover + HPF, three dynamic bars, Dry/Wet readout.
- Right: Out HPF on a dedicated macro column.
- Footer: Input Trim → Dry/Wet → Output Trim → Soft Bypass, matching the suite-wide layout.

## Known Limitations
- Visualization is conceptual (no actual GR meter yet); a sub meter is still on the TODO list.
- Envelope DSP still uses the basic tightness curve; hysteresis + presets landing later in Phase 3.

## Phase 2/3 TODOs
- [ ] Add GR meter or simple sub-level meter for gain staging.
- [ ] Ship preset set (Kick Reinforce 01, 808 Saver 01, etc.).
