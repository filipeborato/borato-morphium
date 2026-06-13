# Borato Morphium — Release v0.3.0

> [!TIP]
> **Borato Morphium** is a "Sound Matter" synthesizer: you don't pick a waveform,
> you pick an **energy source**, send it through a **vibrating object**, and shape it
> as physical **matter** in a **space**. v0.3.0 is the release where the engine stops
> faking that idea and actually implements it — *Energy → Resonance → Matter → Space*.

## ✨ Highlights

- **New primary resonator** — a real Karplus-Strong / digital-waveguide string sits
  between the excitation and the matter stage. Pitch now comes from a vibrating object,
  not from an oscillator. Blendable per preset (`Waveguide` 0 = legacy path, 1 = fully physical).
- **Matter Engine v2** — the modal resonator grew from 4 to **8 modes** across **6 materials**
  (`METAL`, `GLASS`, `WOOD`, `TAPE`, `CIRCUIT`, `CONCRETE`), each with its own
  `{ratio, gain, T60}` table. The matter controls are now physical: `DENSITY` spreads
  inharmonicity, `MASS` tilts the spectrum, `FRICTION` saturates inside the resonant body,
  `WEAR` erodes ring time and detunes modes over time.
- **18 distinct excitation sources** — every SOURCE is now its own model (previously many
  were aliases of the same six renderers):
  `STRIKE`, `IMPULSE`, `PIZZICATO`, `BOW`, `SCRAPE`, `VOICE`, `BREATH`,
  `SPARK`, `SINE`, `NOISE`, `TAPE`, `TAPE LOOP`, plus a six-strong wavetable family.
- **Real wavetable engine + WAVE knob** — a band-limited, mip-mapped single-cycle
  wavetable bank (alias-free at any pitch) with six morphing shapes in the SYNTH family:
  `WT HARMONIC` (sine→square), `WT PWM`, `WT FORMANT` (vowel sweep), `WT METALLIC`,
  `WT EPIANO`, `WT DIGITAL` (PPG-style). A tiny cyan **WAVE** rotary on the scope scans
  the morph frames, and the Motion LFO sweeps the scan for evolving timbres.
- **Sympathetic resonance** — when the waveguide is engaged, a second per-voice string
  rings at a material-dependent interval (fifth/octave/fourth/beating…), adding the
  shimmer of a real resonant body. Per-voice and self-decaying, so it never sticks.
- **A much larger preset library (69 factory presets)** — signature physical-model
  patches (Impulse Bell, Glass Pizzicato, Rosin Cello, Struck Concrete…) plus a
  **32-strong wavetable bank** in a late-80s/90s digital-workstation aesthetic
  (Wavestation / M1 / D-50 / JD-800): vector ROM pads, digital bells, "house"
  electric pianos, sync leads, PCM basses and orchestra hits.

## 🔬 Sound quality & engineering

- **Anti-aliasing** — PolyBLEP band-limiting on the saw (Tape/Tape Loop) and asymmetric
  pulse (Voice), plus a tape record-head rolloff. Non-harmonic energy now stays below
  −30 dBc even on high notes (was clearly audible before).
- **Oversampled drive** — the `DRIVE` tanh now runs at 2× so its harmonics don't fold
  back into the audible band.
- **Per-material space** — the reverb's damping now follows the material (a metal tank
  stays bright, tape/concrete sit dark), and `WEAR` erodes the room further.
- **CPU** — modal coefficients are recomputed at block rate with hysteresis instead of
  per sample (~16× fewer transcendentals); the matter LFO now runs at control rate so
  the parameter smoothers actually ramp.
- **Real-time safety preserved** — all allocation stays in `prepare`; the audio path is
  allocation- and lock-free.

## ✅ Automated spectral test harness (new)

A **Catch2 + CTest** offline suite (`-DMORPHIUM_BUILD_TESTS=ON`) renders the DSP without a
host and asserts on the **FFT**: source f0/aliasing, modal peak placement per material,
mass→centroid monotonicity, wear→decay, no stuck notes, 8-voice headroom, a full
32-preset smoke pass (with WAV dumps for A/B listening), a sample-rate × block-size
matrix, and committed **golden spectra**. The suite runs in CI on Windows, macOS and
Linux (Ubuntu via `xvfb`).

## 🧩 Compatibility

- Existing presets are byte-compatible (legacy oscillator path = `Waveguide` 0; resonator
  modes 0–3 unchanged).
- VST3 / AU / CLAP / Standalone; the manufacturer remains **Borato Company**.

---

*Built with C++20, JUCE 8 and CMake. See `docs/ROADMAP.md` for what's next.*
