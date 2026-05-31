#pragma once

#include <vector>

namespace morphium
{
    /**
        A factory preset: a name plus a concrete value for every parameter.
        Excitation index follows ExcitationType (Bow=0 … Spark=5).
        outputGainDb is in decibels; every other matter/envelope value is the
        parameter's natural range value (seconds for times, 0..1 otherwise).
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
    };

    inline const std::vector<Preset>& getFactoryPresets()
    {
        static const std::vector<Preset> presets =
        {
            //  name                     exc  dens  mass  fric  wear   atk    dec    sus   rel     lfoR   lfoD   revS  revM  resM
            { "INIT DEFAULT",             3,  0.50f,0.50f,0.30f,0.20f, 0.010f,0.200f,0.80f,0.400f,  1.00f, 0.15f, 0.50f,0.15f,  0 },
            { "GLASS COLD ROOM",          7,  0.25f,0.15f,0.20f,0.10f, 0.005f,1.200f,0.20f,1.800f,  0.50f, 0.30f, 0.85f,0.50f,  2 },
            { "COPPER RESONATOR",         0,  0.40f,0.30f,0.35f,0.20f, 0.002f,1.500f,0.00f,1.200f,  1.20f, 0.40f, 0.60f,0.30f,  1 },
            { "WOODEN MARIMBA",           0,  0.55f,0.50f,0.25f,0.30f, 0.001f,0.300f,0.00f,0.400f,  2.50f, 0.10f, 0.40f,0.15f,  3 },
            { "BURNT TAPE LOOP",         11,  0.55f,0.60f,0.60f,0.60f, 0.050f,0.400f,0.70f,0.600f,  4.50f, 0.50f, 0.65f,0.35f,  0 },
            { "CYBER CHAPEL",             5,  0.45f,0.40f,0.30f,0.25f, 0.250f,0.800f,0.85f,1.500f,  0.25f, 0.35f, 0.90f,0.45f,  2 },
            { "SUBMERGED MOTOR",         10,  0.75f,0.85f,0.70f,0.50f, 0.150f,0.600f,0.60f,0.800f,  0.15f, 0.45f, 0.70f,0.25f,  0 },
            { "SPARK STATIC DETROIT",     7,  0.30f,0.25f,0.60f,0.50f, 0.001f,0.250f,0.00f,0.300f,  8.00f, 0.60f, 0.50f,0.20f,  1 },
            
            // --- Novos Presets Musicais ---
            { "GHOSTLY BREATH",           6,  0.20f,0.15f,0.10f,0.40f, 0.800f,1.500f,0.80f,2.000f,  0.10f, 0.60f, 0.95f,0.60f,  2 },
            { "PUMPING SINE BASS",        8,  0.60f,0.80f,0.50f,0.10f, 0.005f,0.350f,0.10f,0.200f,  5.00f, 0.20f, 0.10f,0.05f,  3 },
            { "CINEMATIC SCRAPE",         4,  0.80f,0.70f,0.95f,0.80f, 1.200f,0.500f,0.90f,3.000f,  0.05f, 0.30f, 1.00f,0.75f,  1 },
            { "RETROWAVE PLUCK",          2,  0.30f,0.40f,0.20f,0.30f, 0.002f,0.200f,0.00f,0.250f,  0.00f, 0.00f, 0.40f,0.15f,  0 },
            { "BROKEN CASSETTE",         12,  0.90f,0.80f,0.85f,0.95f, 0.100f,0.500f,0.60f,0.500f,  0.80f, 0.75f, 0.20f,0.10f,  0 },
            { "80S CHOIR SYNTH",          5,  0.40f,0.35f,0.25f,0.15f, 0.080f,0.500f,0.80f,0.600f,  2.50f, 0.25f, 0.60f,0.35f,  2 },
            { "RUSTY ANVIL",              1,  0.75f,0.90f,0.80f,0.70f, 0.001f,0.400f,0.00f,0.500f,  0.00f, 0.00f, 0.30f,0.20f,  1 },
            { "NEON WAVETABLE",           9,  0.35f,0.20f,0.15f,0.05f, 0.010f,0.600f,0.50f,0.400f,  4.00f, 0.50f, 0.50f,0.25f,  0 },
            { "WIND IN THE WIRES",       10,  0.20f,0.10f,0.80f,0.90f, 0.500f,1.000f,0.80f,2.500f,  0.12f, 0.80f, 0.90f,0.50f,  1 },
            { "CELLO ON FIRE",            3,  0.85f,0.75f,0.90f,0.60f, 0.150f,0.300f,0.75f,0.500f,  6.00f, 0.35f, 0.45f,0.20f,  3 },
        };
        return presets;
    }
}
