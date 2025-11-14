# GLS Suite Build Status — Phase 1 Snapshot

Legend: ✅ complete · 🟡 in progress · ⬜ not started · ⚠️ needs attention

| SKU                | Category        | Phase 1 Skeleton | Phase 2 DSP Core | Built (Debug) | vst3validator | Docs Stub | Notes |
|--------------------|-----------------|------------------|------------------|---------------|---------------|-----------|-------|
| GLS.ChannelPilot   | Core Mix/Bus    | ✅               | 🟡               | ⬜             | ⬜             | ✅         | Wire into CMake build.
| GLS.ChannelStripOne| Core Mix/Bus    | ✅               | 🟡               | ⬜             | ⬜             | ✅         | Needs DSP polish.
| MDL.ChopperTrem    | Mod/Delay/LFO   | ✅               | 🟡               | ⬜             | ⬜             | ✅         | Tempo sync validation pending.
| GRD.MixHeat        | Saturation      | ✅               | ✅ placeholder   | ⬜             | ⬜             | ✅         | Ready for presets.
| PIT.ShiftPrime     | Pitch/Time      | ✅               | 🟡               | ⬜             | ⬜             | ✅         | Build + validator next.
| PIT.DoubleStrike   | Pitch/Time      | ✅               | 🟡               | ⬜             | ⬜             | ✅         | Configure dual voice DSP.
| PIT.TimeStack      | Pitch/Time      | ✅               | ⬜               | ⬜             | ⬜             | ✅         | Needs host-sync support.
| PIT.MicroShift     | Pitch/Time      | ✅               | ⬜               | ⬜             | ⬜             | ✅         | Awaiting HQ pitch DSP.
| PIT.ShimmerFall    | Pitch/Time      | ✅               | ⬜               | ⬜             | ⬜             | ✅         | Needs real pitch shifter.
| PIT.GrowlWarp      | Pitch/Time      | ✅               | ⬜               | ⬜             | ⬜             | ✅         | Awaiting burst tests.
| UTL.SignalTracer   | Utility         | ✅               | ⬜               | ⬜             | ⬜             | ✅         | Add tap visualizations later.
| Remaining SKUs     | —               | 🟡               | ⬜               | ⬜             | ⬜             | ⬜         | Tackle in future waves.

_Update this table after each build + validation run._
