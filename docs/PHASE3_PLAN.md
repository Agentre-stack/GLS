# Phase 3 Goals — GLS Suite

Phase 3 focuses on shipping the usability, visualization, and advanced DSP features that were deferred while Phase 2 concentrated on core sonics. The backlog below aggregates the Phase 2/3 TODOs documented inside each plugin brief so we can keep marching “two by two”.

**Progress:** Current Phase 3 cockpit/DSP/preset sweep sits at 100% for the pairs listed below (latest full build + vst3validator run completed). Continue picking the next pair using the same two-by-two cadence.

## Cross-Cutting Objectives
1. **UI polish & preset coverage** — Several plugins still need styled GUIs and curated presets (Channel Pilot calls out UI + presets in `docs/plugins/GLS.ChannelPilot.md:30-33`; Parallel Press wants genre presets per `docs/plugins/GLS.ParallelPress.md:29-35`; Signal Tracer needs tap labeling/presets per `docs/plugins/UTL.SignalTracer.md:24-31`). Provide a shared look-and-feel kit plus a preset-authoring sprint.
2. **Metering & visualization** — Mix and analysis tools lack the meters they describe (Parallel Press GR meter per `docs/plugins/GLS.ParallelPress.md:29-35`; Ambience Evolver LUFS/profile indicators per `docs/plugins/AEV.AmbienceEvolverSuite.md:27-33`; Signal Tracer oscilloscope + correlation views per `docs/plugins/UTL.SignalTracer.md:24-31`). Build reusable JUCE widgets (GR, LUFS, scopes) and hook them into each processor.
3. **Advanced detection / DSP refinements** — Requests include steeper filters + auto gain in Channel Pilot (`docs/plugins/GLS.ChannelPilot.md:30-33`), RMS/peak switch + alternate shelf styles in Dynamic Tilt Pro (`docs/plugins/EQ.DynamicTiltPro.md:25-32`), and external sidechains with state meters in Punch Gate (`docs/plugins/DYN.PunchGate.md:24-30`). Plan the shared DSP upgrades (detector framework, filter blocks) before implementing per plugin.
4. **Workflow & profile management** — Ambience Evolver needs multi-profile capture and stereo profiling (`docs/plugins/AEV.AmbienceEvolverSuite.md:27-33`), while Signal Tracer wants tap labeling/recall. Add lightweight session storage helpers so capture metadata and presets travel with plugin state.

## Plugin-Specific Follow-Up

| Plugin | Phase 3 Tasks | Source Notes |
|--------|---------------|--------------|
| GLS.ChannelPilot | ✅ HPF/LPF slope toggle + auto gain + Goodluck cockpit landed; factory presets added. | `docs/plugins/GLS.ChannelPilot.md:30-33` |
| GLS.BusGlue | ✅ Goodluck cockpit; presets added (Drum Glue/MixBus/Vocal). Auto makeup still future. | `docs/plugins/GLS.BusGlue.md:30-33` |
| GLS.ParallelPress | ✅ Cockpit GR meter + auto makeup logic shipped; presets added (Drum Crush/Vocal Glue/Bus Lift). | `docs/plugins/GLS.ParallelPress.md:29-35` |
| AEV.AmbienceEvolverSuite | ✅ RMS meter + capture indicator delivered; 3-slot stereo profile capture added. | `docs/plugins/AEV.AmbienceEvolverSuite.md:24-33` |
| EQ.BusPaint | ✅ Goodluck cockpit + presets (Drum Bus/Mix Paint/Instrument Glue) added. | `docs/plugins/EQ.BusPaint.md:30-33` |
| EQ.AirGlass | ✅ Air shelf + de-harsh engine with presets (Pop Vocal Air/Master Shimmer/Cymbal Brighten). | `docs/plugins/EQ.AirGlass.md:25-32` |
| EQ.TiltLine | ✅ Tilt + shelves/pivot cockpit with presets (Bright Vocal/Warm Bus/Air Lift). | `docs/plugins/EQ.TiltLine.md` |
| EQ.LowBender | ✅ Low sculptor presets (808 Lift/Bass Guitar/Kick Tight); output/mix remain future. | `docs/plugins/EQ.LowBender.md` |
| EQ.HarmonicEQ | ✅ Harmonic bell with presets (Vocal Air/Synth Shine/Master Glue). | `docs/plugins/EQ.HarmonicEQ.md` |
| EQ.MixNotchLab | ✅ Dual notch lab with listen modes; presets (Vocal Clean/Drum Box Cutter/Mix Fizz Tamer). | `docs/plugins/EQ.MixNotchLab.md` |
| EQ.DynamicTiltPro | ✅ RMS/Peak selector + shelf “Styles” shipped; factory presets (Vocal Pop/Drum Bus/Master Air) added. | `docs/plugins/EQ.DynamicTiltPro.md:25-33` |
| DYN.PunchGate | ✅ Sidechain filters + gate meter shipped; curated Drum/Guitar/Vox preset bank added. | `docs/plugins/DYN.PunchGate.md:24-30` |
| UTL.SignalTracer | ✅ Scope/correlation + tap labeling/presets delivered. Add factory tracing presets if desired. | `docs/plugins/UTL.SignalTracer.md:24-31` |

> **Execution cadence:** keep the proven “two-by-two” rotation—pick two plugins from the table, deliver their Phase 3 items end-to-end (DSP + UI + docs + tests), rerun the single-threaded build + validator, then move to the next pair.
