# Borato Morphium

**Sound Matter Synthesizer** — an experimental VST3 synth built on JUCE.

Instead of picking a waveform, you pick a *source of sonic energy* (Bow, Strike,
Tape, Voice, Noise, Spark), then shape it as physical **matter** (density, mass,
friction, wear). This repository is the MVP **CORE**: a playable, stable, and
extensible foundation.

## Signal flow

```
MIDI note → Voice Engine → Excitation Core → Basic Matter Processor → Output Gain
```

## Project layout

```
CMakeLists.txt              Build system (JUCE via add_subdirectory at C:/JUCE)
BoratoMorphium.jucer        Projucer project (mirrors the CMake setup)
Resources/
  morphium_panel.svg        Panel artwork, embedded as binary data
Source/
  PluginProcessor.*         AudioProcessor, APVTS, juce::Synthesiser host
  PluginEditor.*            UI: SVG panel + controls laid out over its zones
  Presets.h                 Factory preset table (program API)
  Parameters/
    ParameterIDs.h          Single source of truth for parameter IDs
    ParameterLayout.*       APVTS parameter layout
  Core/
    MorphiumSound.h         SynthesiserSound
    MorphiumVoice.*         One polyphonic voice (excitation → matter → ADSR)
    ExcitationCore.*        The six energy sources
  DSP/
    BasicMatterProcessor.*  density / mass / friction / wear shaping
  UI/
    MorphiumLookAndFeel.*   Panel-styled knobs & faders (SVG-ready)
installer/
  BoratoMorphium.iss        Inno Setup script (Windows installer)
```

## Building on Windows (Visual Studio + CMake)

Requires CMake ≥ 3.22, Visual Studio 2022/2026 with the C++ workload, and a JUCE
checkout (auto-detected at `C:/JUCE`).

```powershell
# Configure (generates a .sln)
cmake -B build -G "Visual Studio 18 2026" -A x64
#   …use "Visual Studio 17 2022" if that is your installed toolset.

# Build the VST3
cmake --build build --config Release --target BoratoMorphium_VST3

# Build the Standalone app (fastest way to play/test without a DAW)
cmake --build build --config Release --target BoratoMorphium_Standalone
```

To open the solution in the IDE:

```powershell
start build\BoratoMorphium.sln
```

### Artifacts

```
build/BoratoMorphium_artefacts/Release/VST3/Borato Morphium.vst3
build/BoratoMorphium_artefacts/Release/Standalone/Borato Morphium.exe
```

Set `-DMORPHIUM_COPY_PLUGIN_AFTER_BUILD=ON` to auto-install the VST3 to the
system plugin folder after building.

The Standalone is built with **ASIO** enabled on Windows (JUCE 8 ships a bundled
ASIO header, so no external Steinberg SDK is required). Enabling it means the
binary depends on the ASIO SDK source terms from Steinberg.

### Pointing at a different JUCE

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64 -DMORPHIUM_JUCE_SOURCE_DIR="D:/path/to/JUCE"
```

### Building with Projucer

`BoratoMorphium.jucer` mirrors the CMake project (same sources, same `JUCE_ASIO`
option, VST3 + Standalone). Open it in Projucer, save to regenerate
`Builds/` and `JuceLibraryCode/`, then build the generated Visual Studio
solution. CMake remains the primary, reproducible build.

## Windows installer (Inno Setup)

A single-file installer is produced with [Inno Setup](https://jrsoftware.org/isinfo.php).
The script lives at `installer/BoratoMorphium.iss` and bundles both formats:

- **VST3** → `C:\Program Files\Common Files\VST3\Borato Morphium.vst3`
  (the standard system location DAWs scan automatically).
- **Standalone** → `C:\Program Files\Borato Company\Borato Morphium\`,
  with a Start Menu shortcut and an uninstaller.

Components (VST3 / Standalone) are individually selectable; the wizard is
available in English and Brazilian Portuguese; x64; requires elevation.

Build the plugin in Release first, then compile the installer:

```powershell
# 1) Build the artefacts the installer packages
cmake --build build --config Release --target BoratoMorphium_VST3 BoratoMorphium_Standalone

# 2) Compile the installer (Inno Setup, installed e.g. via: winget install JRSoftware.InnoSetup)
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\BoratoMorphium.iss
```

The result is written to `installer/Output/BoratoMorphium-<version>-Setup.exe`.
Bump `#define MyAppVersion` in the `.iss` (ideally matching the CMake
`project(... VERSION)`) when releasing a new build.

## Real-time safety notes

- All allocation happens in `prepareToPlay` / `prepare`; the audio path is
  allocation- and lock-free.
- Voices read parameters through atomic pointers into the APVTS.
- `juce::SmoothedValue` is used for output gain, velocity, and matter controls
  to avoid zipper noise.
- `juce::ScopedNoDenormals` guards `processBlock`.
