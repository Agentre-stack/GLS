# Bus Paint — EQ.BusPaint

**Category:** EQ / Bus tone  
**Role:** Fast tonal sculptor for mix busses with opposing tilts and musical bells. Includes factory presets.

## Signal Flow

`Input trim → Low shelf tilt → High shelf tilt → Presence bell → Warmth bell → Output trim`

## Parameters

| ID             | Display Name | Notes |
|----------------|--------------|-------|
| `input_trim`   | Input Trim   | Stage gain into the tilt stack (-18 to +18 dB). |
| `low_tilt`     | Low Tilt     | ±6 dB shelf around 150 Hz for weight vs. tightness. |
| `high_tilt`    | High Tilt    | ±6 dB shelf centered ~6 kHz for sheen vs. smoothness. |
| `presence`     | Presence     | ±6 dB bell at 3.2 kHz to add focus or relax mids. |
| `warmth`       | Warmth       | ±6 dB bell at 450 Hz to reinforce glue or scoop mud. |
| `output_trim`  | Output Trim  | Gain staging (-18 to +18 dB). |
| `ui_bypass`    | Soft Bypass  | Goodluck footer bypass (logo/footer). |

## Usage Notes
- Combine small moves across controls (±1–2 dB) to “paint” mix coloration quickly on drum/music busses.
- High Tilt pairs well with slight Presence boost for airy pop buses; counter with Warmth cuts if things get boxy.
- Keep Output Trim near zero and lower it when stacking boosts to avoid clipping following processors.
- The cockpit uses the shared Goodluck header/footer with tilt/presence/warmth macros on top and trims in the bottom row.
- Presets: **Drum Bus** (weight + sheen), **Mix Paint** (balanced mix tilt), **Instrument Glue** (warmth focus).

## Known Limitations
- Shelves/bells currently share fixed Q values; future revisions should make Q adaptive to avoid ringing.
- No per-band bypass yet—automation requires adjusting gain to zero to disable a band.

## Phase 2/3 TODOs
- [ ] Add selectable Q or Style toggles (wide/glue vs. tight/focus).
- [ ] Provide built-in band bypass switches and per-band meters.
- [x] Introduce preset set covering drums, mix bus, instrumental glue.
