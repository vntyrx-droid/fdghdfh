#pragma once

#include <cmath>

namespace chorus
{

enum class LfoShape
{
    sine = 0,
    triangle
};

/** Unipolar-phase LFO producing a bipolar (-1..1) output.

    The phase is kept separately from the output so that several taps can read
    the same oscillator at different offsets. That is how the stereo spread and
    the per-voice distribution are built: one oscillator, many phase offsets,
    which keeps the voices locked in relative phase instead of drifting.
*/
class Lfo
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        updateIncrement();
    }

    void reset (float startPhase = 0.0f) noexcept
    {
        phase = wrap01 (static_cast<double> (startPhase));
    }

    void setRateHz (float newRateHz) noexcept
    {
        rateHz = newRateHz < 0.0f ? 0.0f : newRateHz;
        updateIncrement();
    }

    float getRateHz() const noexcept { return rateHz; }

    void advance() noexcept
    {
        phase = wrap01 (phase + increment);
    }

    /** Current phase in turns, for host-visible state or display. */
    float getPhase() const noexcept { return static_cast<float> (phase); }

    /** Value of the oscillator at the current phase plus an offset in turns (0..1). */
    float valueAt (LfoShape shape, float phaseOffset) const noexcept
    {
        return evaluate (shape, static_cast<float> (wrap01 (phase + static_cast<double> (phaseOffset))));
    }

    static float evaluate (LfoShape shape, float p) noexcept
    {
        if (shape == LfoShape::sine)
            return std::sin (6.283185307179586f * p);

        // Triangle, phase-aligned with the sine: 0 -> 0, 0.25 -> +1, 0.75 -> -1.
        if (p < 0.25f)  return 4.0f * p;
        if (p < 0.75f)  return 2.0f - 4.0f * p;
        return 4.0f * p - 4.0f;
    }

private:
    void updateIncrement() noexcept
    {
        increment = static_cast<double> (rateHz) / sr;
    }

    /** The phase accumulator is double precision on purpose: a float phase
        loses roughly 4e-3 of a cycle per second at 48 kHz, which is enough for
        the three voices to drift out of their intended relative phase over the
        length of a session.
    */
    static double wrap01 (double p) noexcept
    {
        p -= std::floor (p);
        // Guard against floor() returning p itself for tiny negative values.
        return (p < 0.0 || p >= 1.0) ? 0.0 : p;
    }

    double sr        = 44100.0;
    float  rateHz    = 1.0f;
    double increment = 0.0;
    double phase     = 0.0;
};

} // namespace chorus
