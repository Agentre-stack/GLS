# Channel Pilot — GLS.ChannelPilot

**Category:** Core Mix/Bus  
**Role:** Goodluck cockpit for gain, HP/LP cleanup, polarity, pan, auto gain-matching, and factory presets.

## Signal Flow

`Input -> Input Trim -> HPF -> LPF -> Phase invert -> Auto Gain -> Pan -> Output Trim -> Soft Bypass`

## Parameters

| ID            | Display Name | Type  | Range / Units | Notes |
|---------------|--------------|-------|---------------|-------|
| `input_trim`  | Input Trim   | float | -24..+24 dB   | Pre-filter gain staging. |
| `hpf_freq`    | HPF Freq     | float | 20..400 Hz    | High-pass cutoff. |
| `lpf_freq`    | LPF Freq     | float | 4k..20k Hz    | Low-pass cutoff. |
| `phase`       | Phase        | bool  | Off/On        | Invert polarity. |
| `filter_slope`| Filter Slope | choice| 12 or 24 dB/oct | Cascades the HPF/LPF biquads for steeper cleanup. |
| `pan`         | Pan          | float | -1..+1        | Equal-power stereo pan. |
| `output_trim` | Output Trim  | float | -24..+24 dB   | Final gain. |
| `auto_gain`   | Auto Gain    | bool  | Off/On        | Keeps post-filter RMS matched to the input trim. |
| `ui_bypass`   | Soft Bypass  | bool  | Off/On        | Skips processing while leaving trims in place. |

## Usage Notes
- Keep on every channel to standardize level + spectrum before heavier processing; the hero shows HPF/LPF markers and pan needle.
- Phase button is handy for quick polarity checks against buses; the auto-gain meter confirms level matching when filters get steep.
- HPF/LPF are gentle by default—flip to 24 dB/oct when you need harder cleanup without touching downstream balances.
- Presets: Drum Clean (tight low/high cleanup), Vox Prep (higher HPF, gentle slope), Guitar Wide (slight pan spread, lower auto gain).

## Known Limitations
- Filter Q remains fixed; future work could expose broader tone flavours.
