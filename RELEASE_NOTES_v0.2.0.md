# Borato Morphium - Release v0.2.0

> [!TIP]
> **Borato Morphium** is a next-generation Outrun/Synthwave software synthesizer that blends physical modeling with analog-style modulation and a striking neon cyberpunk UI.

## 🚀 Core Features & UI Architecture

The plugin is visually and architecturally divided into 4 main pillars:

### 1. Excitation Core (The Spark)
The engine starts by injecting energy into a virtual physical system. Instead of simple oscillators, Morphium uses **Excitation Sources** like *Strike, Impulse, Pizzicato, Bow, Scrape, Tape, Noise*, and more. 
- The **A.D.S.R Envelope** controls this initial burst of energy in a perfectly aligned 2x2 grid.

### 2. Matter Resonator (The Physical Body)
The excitation signal is fed into a **Modal Resonator Bank**, simulating the acoustic properties of real materials.
- **Modes**: `OFF`, `METAL`, `GLASS`, `WOOD`.
- **Faders**:
  - `DENSITY`: Alters the harmonic spacing of the modes.
  - `MASS`: Shifts the fundamental resonant frequencies.
  - `FRICTION`: Simulates damping (how fast high frequencies decay).
  - `WEAR`: Introduces inharmonicity and chaos into the material structure.

### 3. Borato Macro (The Morph Matrix Engine)
A giant hero knob centrally located. This engine allows interpolating multiple parameters simultaneously. 
- **Modes**: `MATTER`, `SPACE`, `PUNCH`, `CHAOS`, `FREEZE`, `PULSAR`, `LOFI`, `RESONANCE`.

### 4. Motion & Space
- **Motion**: Dual LFO controls (`RATE` and `DEPTH`) to breathe life and drift into the static physical models.
- **Space**: A powerful FDN-style Reverb (`SIZE` and `MIX`).
- **Color**: A dedicated `DRIVE` parameter for analog-style tape saturation and soft-clipping, keeping the signal warm and preventing digital harshness.

---

## 🧮 The Mathematics and Physics Under the Hood

### Modal Synthesis (Matter Resonator)
The physical modeling in Morphium relies on **Modal Synthesis**. 
A physical object (like a metal plate or glass tube) vibrates at specific resonant frequencies called "modes". Mathematically, this is implemented as a parallel bank of highly resonant second-order IIR bandpass filters (biquads).

* **Excitation**: A broad-spectrum signal (like white noise or a short impulse).
* **Transfer Function**: Each mode is a damped harmonic oscillator. The total response is the sum of `N` modes. The frequencies of these modes are governed by the **MASS** and **DENSITY** parameters, while the decay (damping) is governed by **FRICTION**.
* **Wear**: Simulates physical imperfections by adding chaos and slight frequency detuning to the individual modes, making the material sound aged or broken.

### Analog Saturation (Drive)
The `DRIVE` parameter uses a mathematical waveshaper to simulate analog circuitry. Instead of hard digital clipping which causes unpleasant aliasing, Morphium uses a **Hyperbolic Tangent function (tanh)**:

```cpp
y = tanh(x * preGain) * postGain
```

This provides a smooth, gradual compression of the peaks. As the signal approaches the limit, it rounds off gracefully, introducing musical odd-order harmonics that give the synth its characteristic "warm" Synthwave grit.

### Automated UI Mathematics
The UI is strictly bound by mathematical coordinates bridging raw C++ bounds to a complex SVG background. The UI enforces perfect symmetry:
- **Macro Centering**: Centered precisely at `X=792` in a 235px column.
- **Dynamic Displays**: Using `juce::jmin(24.0f, w * 0.08f)` to adapt text and arrow bounds proportionally regardless of the display size, ensuring UI robustness.
