#pragma once

#include <vector>

namespace morphium
{
    /**
        A factory preset: a name plus a concrete value for every parameter.
        Excitation index follows ExcitationType (Strike=0 … WtDigital=17;
        the wavetable family is 9 = WT HARMONIC and 13..17 = PWM/FORMANT/
        METALLIC/EPIANO/DIGITAL).
        outputGainDb is in decibels; every other matter/envelope value is the
        parameter's natural range value (seconds for times, 0..1 otherwise).

        `wavePos` (wavetable scan, 0..1) has a default so the older positional
        rows that omit it fall back to mid-table; only wavetable presets need it.
    */
    struct Preset
    {
        const char* name;
        int   excitation;
        float density, mass, friction, wear;
        float attack, decay, sustain, release;
        float lfoRate, lfoDepth;
        float reverbSize, reverbMix;
        int   resonatorMode;
        float waveguide;        // primary-resonator blend: 0 = legacy oscillator path
        float wavePos = 0.5f;   // wavetable scan position (WAVE knob); mid-table default
    };

    inline const std::vector<Preset>& getFactoryPresets()
    {
        static const std::vector<Preset> presets =
        {
            //  name                     exc  dens  mass  fric  wear   atk    dec    sus   rel     lfoR   lfoD   revS  revM  resM   wg
            { "INIT DEFAULT",             3,  0.50f,0.50f,0.30f,0.20f, 0.010f,0.200f,0.80f,0.400f,  1.00f, 0.15f, 0.50f,0.15f,  0,  0.00f },
            { "GLASS COLD ROOM",          7,  0.25f,0.15f,0.20f,0.10f, 0.005f,1.200f,0.20f,1.800f,  0.50f, 0.30f, 0.85f,0.50f,  2,  0.00f },
            { "COPPER RESONATOR",         0,  0.40f,0.30f,0.35f,0.20f, 0.002f,1.500f,0.00f,1.200f,  1.20f, 0.40f, 0.60f,0.30f,  1,  0.00f },
            { "WOODEN MARIMBA",           0,  0.55f,0.50f,0.25f,0.30f, 0.001f,0.300f,0.00f,0.400f,  2.50f, 0.10f, 0.40f,0.15f,  3,  0.00f },
            { "BURNT TAPE LOOP",         11,  0.55f,0.60f,0.60f,0.60f, 0.050f,0.400f,0.70f,0.600f,  4.50f, 0.50f, 0.65f,0.35f,  4,  0.00f },
            { "CYBER CHAPEL",             5,  0.45f,0.40f,0.30f,0.25f, 0.250f,0.800f,0.85f,1.500f,  0.25f, 0.35f, 0.90f,0.45f,  2,  0.00f },
            { "SUBMERGED MOTOR",         10,  0.75f,0.85f,0.70f,0.50f, 0.150f,0.600f,0.60f,0.800f,  0.15f, 0.45f, 0.70f,0.25f,  6,  0.00f },
            { "SPARK STATIC DETROIT",     7,  0.30f,0.25f,0.60f,0.50f, 0.001f,0.250f,0.00f,0.300f,  8.00f, 0.60f, 0.50f,0.20f,  1,  0.00f },

            // --- Novos Presets Musicais ---
            { "GHOSTLY BREATH",           6,  0.20f,0.15f,0.10f,0.40f, 0.800f,1.500f,0.80f,2.000f,  0.10f, 0.60f, 0.95f,0.60f,  2,  0.00f },
            { "PUMPING SINE BASS",        8,  0.60f,0.80f,0.50f,0.10f, 0.005f,0.350f,0.10f,0.200f,  5.00f, 0.20f, 0.10f,0.05f,  3,  0.00f },
            { "CINEMATIC SCRAPE",         4,  0.80f,0.70f,0.95f,0.80f, 1.200f,0.500f,0.90f,3.000f,  0.05f, 0.30f, 1.00f,0.75f,  1,  0.00f },
            { "RETROWAVE PLUCK",          2,  0.30f,0.40f,0.20f,0.30f, 0.002f,0.200f,0.00f,0.250f,  0.00f, 0.00f, 0.40f,0.15f,  0,  0.00f },
            { "BROKEN CASSETTE",         12,  0.90f,0.80f,0.85f,0.95f, 0.100f,0.500f,0.60f,0.500f,  0.80f, 0.75f, 0.20f,0.10f,  4,  0.00f },
            { "80S CHOIR SYNTH",          5,  0.40f,0.35f,0.25f,0.15f, 0.080f,0.500f,0.80f,0.600f,  2.50f, 0.25f, 0.60f,0.35f,  2,  0.00f },
            { "RUSTY ANVIL",              1,  0.75f,0.90f,0.80f,0.70f, 0.001f,0.400f,0.00f,0.500f,  0.00f, 0.00f, 0.30f,0.20f,  1,  0.00f },
            { "NEON WAVETABLE",           9,  0.35f,0.20f,0.15f,0.05f, 0.010f,0.600f,0.50f,0.400f,  4.00f, 0.50f, 0.50f,0.25f,  5,  0.00f },
            { "WIND IN THE WIRES",       10,  0.20f,0.10f,0.80f,0.90f, 0.500f,1.000f,0.80f,2.500f,  0.12f, 0.80f, 0.90f,0.50f,  1,  0.00f },
            { "CELLO ON FIRE",            3,  0.85f,0.75f,0.90f,0.60f, 0.150f,0.300f,0.75f,0.500f,  6.00f, 0.35f, 0.45f,0.20f,  3,  0.00f },

            // --- Physical-model signature presets (primary waveguide resonator) ---
            { "CONCRETE PIANO",           0,  0.60f,0.55f,0.25f,0.15f, 0.001f,0.800f,0.00f,0.800f,  0.10f, 0.05f, 0.55f,0.30f,  6,  1.00f },
            { "SOFT METAL BOW",           3,  0.55f,0.45f,0.50f,0.20f, 0.400f,0.600f,0.85f,1.200f,  0.30f, 0.25f, 0.70f,0.40f,  1,  1.00f },
            { "MAGNETIC MEMORY",         12,  0.60f,0.50f,0.45f,0.70f, 0.150f,0.500f,0.70f,0.900f,  0.80f, 0.50f, 0.60f,0.35f,  4,  0.80f },
            { "WIRE CHOIR",               5,  0.45f,0.35f,0.30f,0.25f, 0.300f,0.700f,0.80f,1.500f,  0.25f, 0.30f, 0.80f,0.45f,  1,  0.90f },

            // --- Source-showcase presets: each pairs a distinct exciter with
            //     the primary resonator + a matching material. ---
            //  name                     exc  dens  mass  fric  wear   atk    dec    sus   rel     lfoR   lfoD   revS  revM  resM   wg
            { "IMPULSE BELL",             1,  0.45f,0.30f,0.20f,0.10f, 0.001f,1.200f,0.00f,1.500f,  0.20f, 0.10f, 0.60f,0.35f,  1,  1.00f },
            { "GLASS PIZZICATO",          2,  0.30f,0.20f,0.25f,0.10f, 0.001f,0.600f,0.00f,0.700f,  0.30f, 0.10f, 0.70f,0.40f,  2,  1.00f },
            { "ROSIN CELLO",              4,  0.55f,0.50f,0.55f,0.30f, 0.120f,0.400f,0.80f,1.000f,  0.40f, 0.20f, 0.55f,0.30f,  3,  0.85f },
            { "AIR FLUTE",                6,  0.30f,0.25f,0.15f,0.20f, 0.150f,0.400f,0.85f,0.800f,  0.50f, 0.20f, 0.65f,0.40f,  3,  0.60f },
            { "SINE SUB STRING",          8,  0.65f,0.80f,0.30f,0.10f, 0.005f,0.500f,0.30f,0.400f,  0.20f, 0.05f, 0.20f,0.10f,  6,  0.90f },
            { "MORPHING GLASS",           9,  0.35f,0.30f,0.20f,0.15f, 0.050f,0.800f,0.60f,1.000f,  0.30f, 0.30f, 0.70f,0.45f,  2,  0.50f },
            { "WORN LOOP CHAPEL",        12,  0.55f,0.55f,0.50f,0.70f, 0.100f,0.500f,0.70f,1.200f,  0.70f, 0.40f, 0.80f,0.50f,  4,  0.70f },
            { "STRUCK CONCRETE",          0,  0.65f,0.60f,0.30f,0.20f, 0.001f,0.700f,0.00f,0.900f,  0.10f, 0.05f, 0.55f,0.30f,  6,  1.00f },
            { "CIRCUIT HARP",             2,  0.35f,0.30f,0.25f,0.20f, 0.001f,0.500f,0.00f,0.600f,  0.40f, 0.15f, 0.60f,0.35f,  5,  1.00f },
            { "BOWED METAL DRONE",        3,  0.50f,0.40f,0.45f,0.25f, 0.300f,0.600f,0.90f,1.500f,  0.25f, 0.30f, 0.75f,0.45f,  1,  0.90f },

            // --- Wavetable showcase: the WAVE scan is swept by Motion (lfoDepth). ---
            //  name                     exc  dens  mass  fric  wear   atk    dec    sus   rel     lfoR   lfoD   revS  revM  resM   wg
            { "PWM CIRCUIT",             13,  0.40f,0.35f,0.30f,0.20f, 0.010f,0.400f,0.80f,0.500f,  0.40f, 0.60f, 0.55f,0.30f,  5,  0.40f },
            { "VOWEL GLASS PAD",         14,  0.35f,0.30f,0.20f,0.15f, 0.200f,0.600f,0.85f,1.200f,  0.30f, 0.70f, 0.75f,0.45f,  2,  0.40f },
            { "METALLIC LEAD",           15,  0.45f,0.40f,0.35f,0.20f, 0.005f,0.400f,0.70f,0.500f,  0.50f, 0.40f, 0.50f,0.25f,  1,  0.50f },
            { "ELECTRIC TINES",          16,  0.40f,0.45f,0.25f,0.20f, 0.002f,0.500f,0.40f,0.600f,  0.20f, 0.30f, 0.60f,0.35f,  3,  0.50f },
            { "DIGITAL DRONE",           17,  0.45f,0.40f,0.30f,0.25f, 0.300f,0.700f,0.85f,1.400f,  0.25f, 0.55f, 0.70f,0.40f,  5,  0.40f },

            // ================================================================
            //  WAVETABLE BANK — "ROM" era digital workstation aesthetic
            //  (think a Korg/Roland engineer, ~1988-1994: Wavestation, M1,
            //   D-50, JD-800). Lush vector pads, digital bells, house pianos,
            //   sync leads, PCM basses and orchestra hits.
            //  name                     exc  dens  mass  fric  wear   atk    dec    sus   rel     lfoR   lfoD   revS  revM  resM   wg     wpos
            // --- Vector / ROM pads ------------------------------------------
            { "DIGITAL NATIVE",          17,  0.45f,0.40f,0.25f,0.20f, 0.450f,1.000f,0.85f,2.400f,  0.25f, 0.40f, 0.88f,0.55f,  5,  0.30f, 0.55f },
            { "FANTASIA DREAMS",         16,  0.35f,0.30f,0.20f,0.15f, 0.010f,1.400f,0.55f,2.200f,  0.20f, 0.30f, 0.85f,0.55f,  2,  0.35f, 0.60f },
            { "CRYSTAL ROM",             15,  0.30f,0.25f,0.20f,0.10f, 0.200f,1.200f,0.80f,2.000f,  0.35f, 0.45f, 0.85f,0.50f,  2,  0.25f, 0.50f },
            { "VECTOR SUNRISE",           9,  0.45f,0.45f,0.25f,0.20f, 0.600f,1.000f,0.85f,2.400f,  0.15f, 0.55f, 0.82f,0.50f,  5,  0.20f, 0.45f },
            { "AURORA STRINGS",          14,  0.40f,0.40f,0.25f,0.20f, 0.500f,1.000f,0.85f,2.600f,  0.30f, 0.50f, 0.85f,0.55f,  2,  0.30f, 0.55f },
            { "GLASS CATHEDRAL",          9,  0.30f,0.30f,0.20f,0.15f, 0.400f,1.200f,0.80f,2.800f,  0.12f, 0.30f, 0.90f,0.60f,  2,  0.45f, 0.40f },
            { "NEO TOKYO PAD",           13,  0.50f,0.45f,0.30f,0.20f, 0.550f,1.000f,0.85f,2.400f,  0.35f, 0.60f, 0.80f,0.50f,  6,  0.20f, 0.50f },
            { "HEAVENS DOOR",            16,  0.35f,0.35f,0.20f,0.25f, 0.700f,1.200f,0.80f,2.600f,  0.20f, 0.40f, 0.88f,0.58f,  5,  0.30f, 0.55f },
            { "PCM CHOIR",               14,  0.35f,0.35f,0.25f,0.20f, 0.600f,1.000f,0.85f,2.400f,  0.25f, 0.45f, 0.85f,0.55f,  2,  0.25f, 0.65f },
            { "ANALOG SUNSET",           13,  0.50f,0.55f,0.35f,0.25f, 0.500f,1.000f,0.80f,2.200f,  0.30f, 0.55f, 0.78f,0.45f,  3,  0.25f, 0.40f },

            // --- Digital bells / mallets ------------------------------------
            { "DIGITAL BELL ROM",        15,  0.40f,0.25f,0.20f,0.15f, 0.002f,1.600f,0.10f,1.800f,  0.20f, 0.15f, 0.75f,0.45f,  1,  0.40f, 0.70f },
            { "TUBULAR ROM",              9,  0.45f,0.35f,0.25f,0.20f, 0.002f,2.000f,0.00f,2.000f,  0.10f, 0.10f, 0.80f,0.50f,  1,  0.60f, 0.85f },
            { "CHIME OF 89",             15,  0.35f,0.25f,0.20f,0.15f, 0.003f,1.400f,0.15f,1.600f,  0.25f, 0.20f, 0.80f,0.50f,  2,  0.45f, 0.80f },
            { "STAR DUST BELL",          17,  0.35f,0.30f,0.20f,0.15f, 0.005f,1.500f,0.20f,1.800f,  0.40f, 0.35f, 0.85f,0.55f,  2,  0.35f, 0.60f },
            { "MARIMBA ROM",             16,  0.55f,0.50f,0.30f,0.30f, 0.001f,0.500f,0.00f,0.600f,  0.10f, 0.05f, 0.50f,0.25f,  3,  0.55f, 0.35f },
            { "KOTO ROM",                14,  0.45f,0.40f,0.35f,0.30f, 0.001f,0.600f,0.00f,0.700f,  0.10f, 0.10f, 0.55f,0.30f,  3,  0.60f, 0.50f },

            // --- Electric pianos / digital keys -----------------------------
            { "ROM PIANO 88",            16,  0.50f,0.45f,0.25f,0.20f, 0.002f,0.900f,0.35f,0.700f,  0.10f, 0.05f, 0.55f,0.30f,  3,  0.30f, 0.45f },
            { "DIGITALE RHODES",         16,  0.45f,0.50f,0.30f,0.35f, 0.004f,1.000f,0.45f,0.800f,  4.00f, 0.15f, 0.60f,0.35f,  4,  0.25f, 0.40f },
            { "CRYSTAL KEYS",            15,  0.35f,0.30f,0.25f,0.15f, 0.003f,0.800f,0.30f,0.700f,  0.20f, 0.15f, 0.65f,0.40f,  2,  0.30f, 0.55f },
            { "HOUSE STAB 90",           16,  0.45f,0.35f,0.30f,0.20f, 0.002f,0.350f,0.00f,0.300f,  0.10f, 0.05f, 0.45f,0.25f,  5,  0.20f, 0.60f },

            // --- Digital leads ----------------------------------------------
            { "HYPER LEAD",              15,  0.45f,0.35f,0.40f,0.20f, 0.005f,0.400f,0.75f,0.400f,  5.00f, 0.25f, 0.45f,0.20f,  1,  0.30f, 0.75f },
            { "SYNC STAR",               13,  0.45f,0.40f,0.35f,0.20f, 0.010f,0.400f,0.80f,0.500f,  0.40f, 0.70f, 0.50f,0.25f,  5,  0.25f, 0.50f },
            { "DIGITAL SOLO",            17,  0.45f,0.40f,0.35f,0.25f, 0.020f,0.400f,0.80f,0.500f,  5.50f, 0.20f, 0.55f,0.30f,  1,  0.30f, 0.55f },
            { "FORMANT VOX LEAD",        14,  0.40f,0.40f,0.35f,0.20f, 0.030f,0.400f,0.80f,0.500f,  0.50f, 0.65f, 0.55f,0.30f,  2,  0.25f, 0.60f },

            // --- PCM / digital basses ---------------------------------------
            { "DIGI BASS ROM",            9,  0.65f,0.80f,0.35f,0.10f, 0.002f,0.300f,0.50f,0.250f,  0.10f, 0.05f, 0.15f,0.08f,  0,  0.20f, 0.55f },
            { "PWM SUB",                 13,  0.70f,0.85f,0.30f,0.10f, 0.003f,0.350f,0.55f,0.250f,  0.20f, 0.30f, 0.15f,0.08f,  6,  0.25f, 0.45f },
            { "M1 ORGAN BASS",           17,  0.60f,0.75f,0.35f,0.15f, 0.002f,0.300f,0.60f,0.200f,  0.10f, 0.05f, 0.20f,0.10f,  0,  0.15f, 0.60f },
            { "ACID DIGITALE",           13,  0.55f,0.70f,0.55f,0.25f, 0.002f,0.300f,0.30f,0.250f,  3.00f, 0.50f, 0.30f,0.15f,  5,  0.30f, 0.40f },

            // --- Stabs / hits / motion FX -----------------------------------
            { "ORCH HIT 90",             17,  0.60f,0.55f,0.40f,0.30f, 0.002f,0.250f,0.00f,0.350f,  0.10f, 0.05f, 0.60f,0.35f,  6,  0.25f, 0.70f },
            { "VECTOR STAB",             15,  0.45f,0.40f,0.35f,0.20f, 0.002f,0.300f,0.00f,0.350f,  0.10f, 0.05f, 0.55f,0.30f,  5,  0.25f, 0.80f },
            { "ZAP ARP",                 17,  0.40f,0.35f,0.35f,0.20f, 0.001f,0.200f,0.00f,0.250f,  0.10f, 0.10f, 0.50f,0.25f,  5,  0.35f, 0.55f },
            { "WAVE SEQUENCE 88",        17,  0.40f,0.35f,0.25f,0.20f, 0.080f,0.500f,0.75f,1.200f,  0.80f, 0.80f, 0.75f,0.45f,  2,  0.25f, 0.50f },
        };
        return presets;
    }
}
