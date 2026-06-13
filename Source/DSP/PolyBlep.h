#pragma once

namespace morphium
{
    /**
        PolyBLEP residual for band-limiting a unit step discontinuity of size 2
        (the classic Martin Finke formulation).

        Usage:
          saw   = (2*t - 1) - polyBlep (t, dt);
          pulse = (t < duty ? 1 : -1) + polyBlep (t, dt)
                                      - polyBlep (wrap (t - duty), dt);

        t is the normalised phase [0, 1), dt the per-sample phase increment.
        Costs a few FLOPs only in the two samples around each discontinuity.
    */
    inline float polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0f;

        if (t < dt)                       // just after the discontinuity
        {
            const auto x = static_cast<float> (t / dt);
            return x + x - x * x - 1.0f;
        }

        if (t > 1.0 - dt)                 // just before the discontinuity
        {
            const auto x = static_cast<float> ((t - 1.0) / dt);
            return x * x + x + x + 1.0f;
        }

        return 0.0f;
    }
}
