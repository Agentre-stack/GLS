# Stem Balancer — GLS.StemBalancer

**Category:** Core Mix/Bus  
**Role:** Fast finishing EQ macro that tilts, adds presence, tightens lows, and now lives inside the Goodluck cockpit.

## Signal Flow

`Input Trim -> Low Shelf -> High Shelf -> Presence Bell -> Tight HPF -> Stem Gain -> Auto Gain -> Dry/Wet Mix -> Output Trim`

The Phase‑3 editor brings the standard Goodluck structure: left-column macro knobs (Stem Gain, Tilt, Presence, Low Tight), center teal curve visual that previews tilt/presence/HPF, a right column for Auto Gain, and footer trims for Input, Stem Mix, Output, and Soft Bypass.

## Parameters (Phase 2)

| ID         | Display Name | Range / Units | Notes |
|------------|--------------|---------------|-------|
| `stem_gain`| Stem Gain    | -12…+12 dB    | Primary makeup/trim after tone shaping. |
| `tilt`     | Tilt         | -12…+12 dB    | Counter-linked low/high shelves for overall tone. |
| `presence` | Presence     | -6…+6 dB      | Narrow bell around 2.5 kHz for articulation. |
| `low_tight`| Low Tight    | 0…1 (unitless)| Maps to 20–160 Hz HPF to clear rumble. |
| `auto_gain`| Auto Gain    | On/Off        | Optional RMS-based loudness compensation. |
| `mix`      | Stem Mix     | 0…100 %       | Final blend between processed stem macro and the untouched input. |
| `input_trim` | Input      | -24…+24 dB    | Pre-EQ trim located in the cockpit footer. |
| `output_trim`| Output     | -24…+24 dB    | Post chain trim for fast level matching. |
| `ui_bypass` | Soft Bypass | On/Off        | Goodluck bypass that keeps internal state ready for A/B checks. |

## Usage Notes
- Use Tilt for broad tonal moves; ±2 dB is usually enough for stems.
- Presence helps vocals/instruments poke through before reaching for full EQs.
- Low Tight at 0.6–0.7 is great for bus clean-up before sending to limiting.
- Auto Gain keeps loudness stable when printing quick “approval” passes.
- The teal response graph shows the tilt slope, presence bump, and where Low Tight is clamping. Use it to quickly visualize how the stem macro is shaping the spectrum.
- Use the footer Stem Mix to dial in “50% cleanup / 50% original” blends when committing stems for recalls.

## Known Limitations
- Filters are still fixed-Q; exposing alternative presence shapes remains future work.
- LUFS/RMS metering is still pending; current hero display focuses on EQ preview + auto gain status.

## Phase 2/3 TODOs
- [ ] Add LUFS-based metering plus auto gain display (visual EQ curve shipped; loudness meter still outstanding).
- [ ] Offer preset set per stem type (Drums, Vox, Guitars, FX).
