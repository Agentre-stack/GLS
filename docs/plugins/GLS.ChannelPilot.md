# Channel Pilot — GLS.ChannelPilot

**Category:** Core Mix/Bus  
**Role:** Simple gain/pan/HP-LP utility for normalizing every channel.

## Signal Flow

`Input -> HPF -> LPF -> Phase invert -> Gain -> Pan -> Output`

## Parameters

| ID            | Display Name | Type  | Range / Units | Notes |
|---------------|--------------|-------|---------------|-------|
| `input_trim`  | Input Trim   | float | -24..+24 dB   | Pre-filter gain staging. |
| `hpf_freq`    | HPF Freq     | float | 20..400 Hz    | High-pass cutoff. |
| `lpf_freq`    | LPF Freq     | float | 4k..20k Hz    | Low-pass cutoff. |
| `phase`       | Phase        | bool  | Off/On        | Invert polarity. |
| `pan`         | Pan          | float | -1..+1        | Equal-power stereo pan. |
| `output_trim` | Output Trim  | float | -24..+24 dB   | Final gain. |

## Usage Notes
- Keep on every channel to standardize level + spectrum before heavier processing.
- Phase button is handy for quick polarity checks against buses.
- HPF/LPF are gentle 12 dB/oct filters for cleaning rumble/fizz.

## Known Limitations (Phase 1)
- Filters are first-pass placeholders; add oversampling + steeper options later.
- No presets yet; UI is generic JUCE look.

## Phase 2/3 TODOs
- [ ] Improve filter slopes + Q options.
- [ ] Add auto gain-match toggle.
- [ ] Style UI and add factory presets.
