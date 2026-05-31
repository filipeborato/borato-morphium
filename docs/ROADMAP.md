# Borato Morphium — Roadmap & Status

A continuation brief for the project: where it stands today and what is still
missing to match the full concept. Status markers: ✅ done · 🟡 partial · ❌ missing.

## Concept

**Borato Morphium** is a VST3 / Standalone synthesizer of **sound matter** (not
of waveforms). Stack: **C++20, JUCE 8, CMake (primary) + Projucer (mirror),
VST3 + Standalone with ASIO, Windows / MSVC**.

Signal philosophy: `Source → Physical deformation → Morphology → Space → Performance`.

> *"Borato Morphium is not a synthesizer of waveforms. It is a synthesizer of sound matter."*

Clean, modular, real-time-safe architecture (no allocation on the audio thread).
UI is a "vintage lab" panel (cream chassis, knobs, vertical sliders, red LED
display, screws, engraved wordmark) drawn from an embedded SVG, with interactive
JUCE controls and a custom `LookAndFeel`.

## ✅ Implemented (playable, stable base)

**Infra / build**
- ✅ Working CMake project + `.jucer`; VST3 + Standalone (Release) builds validated.
- ✅ ASIO on the Standalone (JUCE 8 bundled ASIO header, no external SDK).
- ✅ Windows installer (Inno Setup): VST3 → `Common Files\VST3`, Standalone →
  Program Files + Start Menu shortcut + uninstaller, EN / PT-BR wizard.
- ✅ Git initialised, `.gitignore`, first commit; English README.

**Audio engine**
- ✅ Polyphonic synth (`juce::Synthesiser`, 8 voices), per-voice ADSR, APVTS,
  output gain, parameter smoothing.
- 🟡 **SOURCE** — six sources exist as *simple stylised excitation*: Bow, Strike,
  Tape, Voice, Noise, Spark.
- 🟡 **MATTER** — `BasicMatterProcessor` with **density / mass / friction / wear**
  → HP/LP (TPT) filters, `tanh` saturation, instability + damping. Generic;
  **no real material models yet**.
- 🟡 **MORPH (Borato Macro)** — hero knob morphing the *matter* through three
  curated states A→B→C (air/glass → body/wood → mass/metal) with easing. Moves
  only the four matter params.
- ✅ **Presets** — 8 factory presets via the real program API (persisted),
  clickable LED display (prev/next).
- ✅ Animated per-source oscilloscope; resizable UI (0.55×–3×, aspect-locked).

## ❌ Gaps to reach the full concept

**SOURCE** — missing: real Sine/**Wavetable**, **Sample player**, Impulse,
**Breath/Blow**, **Scrape**, **Tape Loop**, **Voice Grain / granular** (user
material). The current six should become rich generators, not approximations.

**MATTER (the heart of the concept)** — the main missing piece:
- ❌ **Modal resonator bank** (Metal: inharmonic/ringing; Glass; Wood) — today
  there are only HP/LP filters.
- ❌ Real material models: **Metal, Glass, Wood, Tape (wow/flutter/hiss),
  Circuit (drift/clipping), Air/Water/Concrete/Skin/Wire**.
- ❌ Missing physical controls: **Tension, Age, Instability, Grain, Resonance,
  Temperature** (we only have density/mass/friction/wear).
- ❌ Dispersion, irregular comb filtering, spectral modulation.

**MORPH** — ❌ a real **Morph Matrix** between 3–4 *whole-engine snapshots* (not
just matter); ❌ per-parameter hysteresis / delays; ❌ macro reaching
excitation / formants / grains / diffusion.

**MOTION** — ❌ **absent**: organic LFOs, random walk, analog drift, envelope
followers, probabilistic sequencer, drawn gestures, controlled chaos. (The
"MOTION/SPACE" UI zone currently only hosts Release/Output.)

**SPACE** — ❌ **absent**: **FDN** reverb, early reflections, diffusion network,
modal room, plate, tape echo, optional convolution, **material rooms**
(metal/concrete/glass).

**ANALYZE & RESYNTHESIZE** (the killer feature) — ❌ absent: audio drag-and-drop,
FFT / onset / peak-picking, envelope / spectral-centroid / noisiness extraction,
automatic granular/modal resynthesis → instantly playable preset.

**UI** — ❌ patch-style modulation matrix; ❌ VU meter; ❌ real MOTION/SPACE
controls on the panel; ❌ photorealistic skin (v1.0).

## Roadmap position

We are at a **partial v0.1**: SOURCE (simple) + MATTER (generic) + MACRO +
presets + UI + output are in place, but to *close* v0.1 we still need the modal
resonator bank, a sample/granular player, wavetables, an **FDN reverb**, and
**LFO/drift (MOTION)**. v0.2 (import + DSP analysis + resynthesis) and v1.0
(full engine + matrix + skin) are wide open.

### Original version targets
- **v0.1:** 2 simple WT oscillators + 1 sample/granular player + 1 noise source +
  1 modal resonator bank + 1 multimode filter + tape/circuit saturation + Morph
  macro (4 snapshots) + LFO/random drift + simple FDN reverb + vintage-lab UI.
- **v0.2:** sample import + basic DSP analysis + envelope/spectrum extraction +
  granular resynthesis + "Matter" presets.
- **v1.0:** full engine + total material morphing + modulation matrix +
  performance macro + signature presets + high-end photorealistic skin.

## 🎯 Suggested next steps (to close v0.1)

1. **Modal resonator bank** (`ModalResonator` class) wired into the matter stage
   as a selectable "Metal / Glass / Wood" model — the biggest leap in sonic
   identity.
2. **MOTION**: 2 LFOs + 1 random-walk/drift with a minimal matrix (destinations:
   matter params, pitch, macro).
3. **SPACE**: a simple **FDN** reverb + material rooms — closing the "→ Space" flow.
4. **Real MORPH matrix**: A/B/C snapshots of *all* engine params (not just
   matter), with interpolation.
5. **Sample / granular player** as a new SOURCE.
6. Rewrite the 8 presets toward the signature concept list (*Glass in a Cold
   Chamber, Submerged Motor, Burnt Tape, Concrete Piano…*) using the new models.

## Signature preset concepts (target identity)

*Vidro em Câmara Fria · Motor Submerso · Fita Queimada · Piano de Concreto ·
Cobre Oxidado · Respiração de Máquina · Ruído de Capela · Arco em Metal Mole ·
Memória Magnética · Órgão de Circuito · Coral de Fios · Placa Ressonante nº1.*
