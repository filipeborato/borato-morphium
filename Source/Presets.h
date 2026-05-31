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
        float outputGainDb;
    };

    inline const std::vector<Preset>& getFactoryPresets()
    {
        static const std::vector<Preset> presets =
        {
            //  name              exc  dens  mass  fric  wear   atk    dec    sus   rel    gain
            { "INIT DEFAULT",      0,  0.50f,0.50f,0.30f,0.20f, 0.010f,0.200f,0.80f,0.400f, -6.0f },
            { "GLASS COLD ROOM",   5,  0.25f,0.15f,0.20f,0.10f, 0.005f,1.200f,0.20f,1.800f, -7.0f },
            { "BOWED RESIN",       0,  0.60f,0.55f,0.45f,0.15f, 0.400f,0.600f,0.85f,0.800f, -6.0f },
            { "STRUCK METAL",      1,  0.40f,0.30f,0.35f,0.20f, 0.002f,1.500f,0.00f,1.200f, -7.0f },
            { "TAPE MEMORY",       2,  0.55f,0.60f,0.50f,0.60f, 0.050f,0.400f,0.70f,0.600f, -6.0f },
            { "HOLLOW VOICE",      3,  0.45f,0.40f,0.30f,0.25f, 0.150f,0.300f,0.75f,0.500f, -6.0f },
            { "GREY NOISE BED",    4,  0.50f,0.70f,0.20f,0.40f, 0.600f,1.000f,0.60f,1.500f, -8.0f },
            { "SPARK STATIC",      5,  0.30f,0.25f,0.60f,0.50f, 0.001f,0.250f,0.00f,0.300f, -7.0f },
        };
        return presets;
    }
}
