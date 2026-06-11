# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Borato Morphium — an experimental "sound matter" VST3/Standalone synthesizer built with C++20, JUCE 8, and CMake on Windows/MSVC. Instead of waveforms, the user picks an energy source (Bow, Strike, Tape, Voice, Noise, Spark) and shapes it as physical matter (density, mass, friction, wear). Current state and gaps are tracked in `docs/ROADMAP.md`.

## Build commands

Requires CMake ≥ 3.22, Visual Studio 2022/2026 (C++ workload), and a JUCE checkout auto-detected at `C:/JUCE` (override with `-DMORPHIUM_JUCE_SOURCE_DIR=...`, or `-DMORPHIUM_FETCH_JUCE=ON` to download).

```powershell
# Configure (generates build/BoratoMorphium.sln)
cmake -B build -G "Visual Studio 18 2026" -A x64    # or "Visual Studio 17 2022"

# Build
cmake --build build --config Release --target BoratoMorphium_VST3
cmake --build build --config Release --target BoratoMorphium_Standalone   # fastest way to play/test
```

Artifacts land in `build/BoratoMorphium_artefacts/Release/{VST3,Standalone}/`. `-DMORPHIUM_COPY_PLUGIN_AFTER_BUILD=ON` auto-installs the VST3 to the system folder.

Windows installer (after a Release build of both targets):

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\BoratoMorphium.iss
```

When releasing, bump the version in `CMakeLists.txt` (`project(... VERSION)`) — CI reads it from there and also injects it into the Inno Setup script (whose `#define MyAppVersion` is only a local fallback).

## CI (GitHub Actions)

`.github/workflows/build-release-artifacts.yml` builds on every push to `main`/`release`/`ci/**` and on `v*` tags: Windows x64 (VST3 + Standalone + CLAP + Inno Setup installer), macOS arm64 + Intel (VST3 + AU + CLAP + Standalone, with mandatory `auval -v aumu Mph1 Bora`), and Ubuntu (VST3 + CLAP + Standalone). Tag pushes attach all artifacts to a draft GitHub Release. CLAP is built via clap-juce-extensions, enabled with `-DMORPHIUM_BUILD_CLAP=ON` (OFF by default locally).

There are no automated tests; verification is building the Standalone and playing it.

Note: `BoratoMorphium.jucer` (Projucer) mirrors the CMake project, but **CMake is the primary, reproducible build**. New source files must be added to `target_sources` in `CMakeLists.txt` (and ideally to the `.jucer` to keep the mirror in sync).

## Architecture

Signal flow: `MIDI note → Voice Engine → Excitation Core → Basic Matter Processor → ADSR → Reverb/Drive → Output Gain`.

- `Source/PluginProcessor.*` — `juce::AudioProcessor` hosting an 8-voice `juce::Synthesiser`, the APVTS, the global reverb/drive/output stage, and the factory-preset program API (presets defined in `Source/Presets.h`, persisted via `PresetManager`).
- `Source/Parameters/ParameterIDs.h` — **single source of truth for parameter IDs** (namespace `morphium::params`). Always reference IDs from here; never inline strings. New parameters also go into `Source/Parameters/ParameterLayout.cpp`.
- `Source/Core/MorphiumVoice.*` — one polyphonic voice (excitation → matter → ADSR). Voices read parameters through `VoiceParameters`, a struct of `std::atomic<float>*` pointers wired once by the processor into the APVTS raw values — this is how parameter data crosses to the audio thread, lock-free.
- `Source/Core/ExcitationCore.*` — the six energy sources.
- `Source/DSP/BasicMatterProcessor.*` — density/mass/friction/wear shaping (TPT HP/LP filters, tanh saturation, instability/damping) plus the resonator mode.
- The "Borato Macro" morph (A→B→C matter states) is applied from the editor (`applyMacro`), which sets the four matter parameters.

### Real-time safety (non-negotiable)

- All allocation happens in `prepareToPlay`/`prepare`; the audio path is allocation- and lock-free.
- Parameters reach the audio thread only via atomic pointers into the APVTS (`VoiceParameters`).
- Use `juce::SmoothedValue` for anything audible that changes (gain, velocity, matter controls) to avoid zipper noise; `juce::ScopedNoDenormals` guards `processBlock`.

### UI architecture

The panel is an SVG (`Resources/morphium_panel.svg`, embedded as `BinaryData`) drawn as the editor background. Division of labor:

- **SVG = static chrome only**: chassis, labels, grooves, knob bezels/rings.
- **JUCE (`Source/UI/MorphiumLookAndFeel.*` + custom components in `PluginEditor.h`) = everything interactive**: knob bodies, moving fader caps, the 6 SOURCE buttons with LED, resonator buttons, LED preset display, animated oscilloscope, macro hero knob.
- Avoid double-drawing: if JUCE renders a moving part, the static version must not exist in the SVG.

Controls are positioned in original SVG coordinates (1280×760) via the editor's `svg(x, y, w, h)` helper; the editor is resizable (aspect-locked, 0.55×–3×).

**GUI layout work**: diagnose overlaps/margins by cross-referencing the `svg(...)` bounds in `PluginEditor.cpp` against the SVG element geometry (coordinates, transforms, label baselines, knob radii). Do **not** launch the standalone and screen-capture it — it's slow and the user dislikes it. If a rendered view is truly needed, read the latest screenshot the user drops in `C:\Users\filipe\Pictures\Screenshots`.

`python tools/layout_audit.py` automates this: it parses the `setBounds(svg(...))` calls and the SVG's absolute geometry, reports anchor misalignment / overlaps / label collisions / row-column drift in SVG units to the console, and regenerates `tools/layout_report.html` (panel SVG + canvas overlay of all JUCE bounds, with an interactive findings table). Run it after any change to `resized()` or the panel SVG; the feedback loop is edit → run → refresh browser.
