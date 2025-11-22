# Monoize Pro — GLS.MonoizePro

**Category:** Core Mix/Bus  
**Role:** Low-end mono-izer and stereo sculptor for mastering and live playback prep.

## Signal Flow

`Input -> Mid/Side Split -> Lowpass/Highpass on Side -> Width & Trim -> Recombine`

## Parameters (Phase 2)

| ID            | Display Name | Range / Units | Notes |
|---------------|--------------|---------------|-------|
| `mono_below`  | Mono Below   | 40…400 Hz     | Frequencies below this are folded to mono. |
| `stereo_above`| Stereo Above | 1 kHz…12 kHz  | Upper band emphasis; adds extra width above this point. |
| `width`       | Width        | 0…2           | Side gain multiplier. |
| `center_lift` | Center Lift  | -12…+12 dB    | Gain for the mono Mid. |
| `side_trim`   | Side Trim    | -12…+12 dB    | Gain for the Sides before width scaling. |
| `mix`         | Dry/Wet      | 0…1           | Wet/dry blend (footer control). |
| `input_trim`  | Input Trim   | -24…+24 dB    | Gain staging before mono processing. |
| `output_trim` | Output Trim  | -24…+24 dB    | Post blend trim for loudness matching. |
| `ui_bypass`   | Soft Bypass  | On/Off        | Goodluck footer bypass toggle. |

## Usage Notes
- Use Mono Below ≈120 Hz to tighten club mixes; 60 Hz for mastering.
- Stereo Above >4 kHz adds sparkle without smearing vocals.
- Width >1.2 can cause phase issues; check mono compatibility.

## Phase 3 UI Snapshot
- Teal Goodluck cockpit with macro knobs for Mono Below / Stereo Above / Width and mid/side meter columns showing current trims.
- Footer houses Input Trim → Dry/Wet → Output Trim → Soft Bypass for club-friendly A/B.
- Hero visual plots mono vs stereo thresholds so you can see where folding happens.

## Known Limitations
- Filters remain fixed-order IIR (gentle slopes) for musical transitions.
- No correlation meter yet; external goniometer still recommended.

## Phase 2/3 TODOs
- [ ] Add mid/side correlation meter and preset bank.
- [ ] Optional external sidechain or dynamic width control.
