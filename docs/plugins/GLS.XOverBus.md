# XOver Bus — GLS.XOverBus

**Category:** Core Mix/Bus  
**Role:** Three-way crossover router with adjustable slopes, solos, and output trim for multiband workflows.

## Signal Flow

`Input -> Split Freq 1 -> (Low / Mid) -> Split Freq 2 -> (Mid / High) -> Summing Matrix -> Output Trim`

## Parameters (Phase 2)

| ID            | Display Name | Range / Units | Notes |
|---------------|--------------|---------------|-------|
| `split_freq1` | Split Freq 1 | 50…8000 Hz    | Low/Mid split frequency. |
| `split_freq2` | Split Freq 2 | 50…8000 Hz    | Mid/High split frequency. |
| `slope`       | Slope        | 6…48 dB/Oct   | Linkwitz-Riley slope per band (6 dB steps). |
| `band_solo1`  | Band 1 Solo  | On/Off        | Listen-only low band. |
| `band_solo2`  | Band 2 Solo  | On/Off        | Listen-only mid band. |
| `band_solo3`  | Band 3 Solo  | On/Off        | Listen-only high band. |
| `output_trim` | Output Trim  | -12…+12 dB    | Post-sum gain control. |
| `mix`         | Dry/Wet      | 0…1           | Crossfade between processed crossover sum and the untouched input. |
| `input_trim`  | Input Trim   | -24…+24 dB    | Pre-split staging to keep the filters happy. |
| `ui_bypass`   | Soft Bypass  | On/Off        | Footer toggle for latency-safe auditioning. |

## Usage Notes
- Use 24 dB slopes for master-bus style crossovers; 12 dB for gentle tone splits.
- Solo buttons help route each band to external processing or simply audition.
- Keep split2 higher than split1; validator will pass but overlapping bands can sound strange.
- Output trim ensures unity after recombining; watch for double gains when soloing.
- Dry/Wet and Input Trim live in the Goodluck footer so you can treat XOver Bus like a macro multiband insert (blend back into dry easily).
- Soft Bypass in the footer makes it painless to check DAW bypass vs the suite's own bypass.

## Phase 3 UI Snapshot

- Goodluck header/footer with teal accent, preset label, and A/B-ready strip.
- Macro column: Split Freq 1, Split Freq 2, Slope (large knobs).
- Center visual: teal vertical lines for both splits, slope readout, trio of solo indicators.
- Right column: Low/Mid/High Solo toggles styled as Goodluck chips.
- Footer: Input Trim → Dry/Wet → Output Trim → Soft Bypass, matching the suite spec.

## Known Limitations
- Still sums to stereo—in-band sends/outs remain on the backlog.
- Visual is instructional (no live level meters yet); rely on DAW meters per band.

## Phase 2/3 TODOs
- [ ] Add per-band level meters and send levels.
- [ ] Allow routing each band to discrete outputs for parallel chains.
