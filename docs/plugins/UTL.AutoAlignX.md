# Auto Align X — UTL.AutoAlignX

**Category:** Utility  
**Role:** Channel alignment cockpit with per-channel delay/polarity, trims, and a correlation readout.

## Signal Flow

`Input -> Input Trim -> Left/Right delay lines + polarity flip -> Correlation meter -> Output Trim -> Soft Bypass`

## Parameters
- `delay_left`: Delay applied to the left channel (ms).
- `delay_right`: Delay applied to the right channel (ms).
- `invert_left`: Flips polarity on the left channel.
- `invert_right`: Flips polarity on the right channel.
- `input_trim`: Pre-delay gain staging (-24..+24 dB).
- `output_trim`: Post-delay output level (-24..+24 dB).
- `ui_bypass`: Soft bypass that leaves trims untouched.

## Usage Notes
- Use small delays (<5 ms) to compensate for mic spacing or DI/Reamp latency; the hero bar shows per-channel delay and live RMS.
- Combine delay with polarity flip to cancel comb filtering on multi-mic setups, then check the correlation meter to confirm alignment.
- Input/Output trims in the footer keep level consistent when nudging alignment values mid-session.
