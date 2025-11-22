# Guerilla Verb — AEV.GuerillaVerb

**Category:** Creative Reverb  
**Role:** Hybrid algorithmic + diffusion reverb with morphing, modulation, and IR-esque blend.

## Signal Flow

`Input -> Pre-Delay/Mod -> Early Reflections -> Algorithmic Tank -> Diffusion Bank -> Filters/Color -> Morph -> Width/Mix`

## Parameters (Phase 2)

| ID        | Display Name | Range / Units | Notes |
|-----------|--------------|---------------|-------|
| `size`    | Size         | 0.1…1.0       | Room size factor (feeds tank + diffusion). |
| `predelay`| PreDelay     | 0…200 ms      | Input delay before ER/tank. |
| `decay`   | Decay        | 0.1…15 s      | Overall tail time. |
| `er_level`| ER Level     | 0…1           | Balance of early reflections. |
| `density` | Density      | 0…1           | Diffusion feedback amount. |
| `damping` | Damping      | 0…1           | High-frequency roll-off inside tank/diffusers. |
| `mod_depth`/`mod_rate` | Mod Depth / Rate | 0…1 / 0.05…10 Hz | Pre-delay modulation for smear/chorus. |
| `color`   | Color        | -1…1          | Adds saturation tilt (negative = darker). |
| `hpf`/`lpf`| HPF / LPF  | 20…2k / 2k…20k Hz | Wet path filtering. |
| `width`   | Width        | 0…1.5         | Mid/side width multiplier. |
| `ab_morph`| A/B Morph    | 0…1           | Crossfade between wet tank and pre-delay snapshot. |
| `ir_blend`| IR Blend     | 0…1           | Mix of diffusion “convolution” vs algorithmic result. |
| `mix`     | Mix          | 0…1           | Wet/dry balance. |

## Usage Notes
- Size ~0.6, Decay 4 s, Density 0.5 gives lush plates; drive IR Blend up for gritty rooms.
- Keep Mod Depth under 0.3 for natural spaces, crank for chorusy pads.
- A/B Morph = 0 keeps pure reverb; blending toward 1 brings back transient detail.

## Known Limitations
- Heavy modulation plus IR Blend >0.8 can clip if Mix is maxed; watch wet gain.
- No per-band EQ yet—use downstream EQ for detailed shaping.

## Phase 2/3 TODOs
- [ ] Add wet level control independent from mix for gain staging.
- [ ] Optional multi-out (direct ER, tail, diffusion) for Dolby/Atmos workflows.
