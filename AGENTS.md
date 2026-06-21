# AGENTS.md

C++20 / JUCE 8 / CMake audio plugin project. See `CLAUDE.md` for full architecture details.

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64   # CI uses VS 2022; local may be VS 2026
cmake --build build --config Release --target BoratoMorphium_VST3
cmake --build build --config Release --target BoratoMorphium_Standalone
```

JUCE auto-detected at `C:/JUCE`; override with `-DMORPHIUM_JUCE_SOURCE_DIR=<path>` or `-DMORPHIUM_FETCH_JUCE=ON`.

Artifacts: `build/BoratoMorphium_artefacts/Release/{VST3,Standalone}/`.

## Tests

Tests are OFF by default. Enable explicitly:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DMORPHIUM_BUILD_TESTS=ON
cmake --build build --config Debug --target MorphiumTests
ctest --test-dir build -C Debug --output-on-failure
```

Catch2 is fetched at configure time. Test sources live in `tests/`. The test executable initialises JUCE's MessageManager (needs Xvfb on Linux).

## Adding source files

New `.cpp`/`.h` files **must** be added to `target_sources(BoratoMorphium ...)` in `CMakeLists.txt` (line ~101). Also add to the MorphiumTests target if test code references them. Ideally mirror in `BoratoMorphium.jucer` too — CMake is the primary build; the `.jucer` is a convenience mirror.

## Key conventions

- **Parameter IDs**: single source of truth at `Source/Parameters/ParameterIDs.h` (`morphium::params` namespace). Never inline parameter ID strings.
- **Real-time safety**: no allocation/locks on the audio thread. Parameters cross via `std::atomic<float>*` pointers in `VoiceParameters`. Use `juce::SmoothedValue` for audible changes. `juce::ScopedNoDenormals` in `processBlock`.
- **UI layout**: SVG at `Resources/morphium_panel.svg` is static chrome only. JUCE components in `Source/UI/` handle all interactive parts. Position controls via `svg(x, y, w, h)` in 1280x760 SVG coordinates.
- **Layout debugging**: run `python tools/layout_audit.py` after changing `resized()` or the panel SVG — it reports overlaps/misalignments and regenerates `tools/layout_report.html`. Do **not** launch the standalone to visually check layout.
- **CLAP format**: OFF by default locally; CI enables with `-DMORPHIUM_BUILD_CLAP=ON`.
- **Version**: single source of truth is `project(BoratoMorphium VERSION ...)` in `CMakeLists.txt`. CI reads it at build time. The Inno Setup `#define MyAppVersion` is a local fallback only.
- **CI runner quirk**: Windows CI is pinned to `windows-2022` with "Visual Studio 17 2022" generator. Don't assume CI matches local VS version.
