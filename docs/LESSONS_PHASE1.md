# Lessons Learned — Phase 1 (Scaffolding)

1. **Directory First, Files Second**
   - Always `mkdir -p src/<Namespace>/<Product>` before writing files.
   - Run `wc -l` or `ls` immediately to confirm files landed; shell noise from ~/.bash_profile can hide path typos.

2. **Shell Noise Masks Failures**
   - The pyenv/zsh hooks in `.bash_profile` print errors on every command. Keep automation-friendly shells clean so missing-file errors stand out.

3. **Instant CMake Wiring**
   - Every new plugin needs a matching `CMakeLists.txt` and `add_subdirectory(...)` entry the moment it is created. Otherwise code sits unbuilt and regressions pile up.

4. **APVTS Consistency Is Gold**
   - Parameter IDs mirror CSV `key_controls` using lowercase_with_underscores. This keeps editors, automation, and docs aligned across 75 SKUs.

5. **Two-at-a-Time Workflow Wins**
   - Working in pairs (e.g., ShiftPrime + DoubleStrike) balances momentum vs. review load. Adopt this cadence for future DSP/UI waves.

6. **Documentation + Tooling Early**
   - Adding doc stubs, changelog entries, and validator scripts now prevents chaotic catch-up later.
