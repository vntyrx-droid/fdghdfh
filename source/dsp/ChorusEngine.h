#pragma once

#include "DelayLine.h"
#include "Lfo.h"

#include <array>

namespace chorus
{

/** Three-voice modulated-delay chorus.

    Signal flow, per channel: the dry input feeds a delay line whose read
    position is swept by up to three LFOs. Each enabled voice contributes one
    tap; the taps are averaged so that enabling a voice changes the texture
    rather than the level. Stereo width comes from reading the same LFOs at a
    phase offset on the right channel, which is what gives a chorus its
    characteristic side-to-side motion without decorrelating the bass.

    There is deliberately no feedback path: feedback turns a chorus into a
    flanger, and the control set here targets classic chorus behaviour.
*/
class ChorusEngine
{
public:
    static constexpr int   numVoices    = 3;
    static constexpr float maxDelayMs   = 40.0f;
    static constexpr float maxDepthMs   = 20.0f;
    static constexpr float maxRateHz    = 10.0f;

    struct Parameters
    {
        float    delayMs  = 12.0f;  ///< Base (unmodulated) delay.
        float    depthMs  = 6.0f;   ///< Peak sweep added on top of the base delay.
        float    stereo   = 1.0f;   ///< 0 = mono-phase, 1 = 180 degrees between L and R.
        float    mix      = 0.5f;   ///< Dry/wet blend, ignored when wetOnly is set.
        bool     wetOnly  = false;
        LfoShape shape    = LfoShape::sine;

        std::array<float, numVoices> rateHz  { 0.35f, 0.83f, 1.61f };
        std::array<bool,  numVoices> enabled { true,  true,  false };
    };

    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    /** Sets the target parameters. Continuous values are smoothed internally,
        so this is safe to call every block from the audio thread.
    */
    void setParameters (const Parameters& newParams);

    /** Processes in place. right may be null for mono operation. */
    void process (float* left, float* right, int numSamples);

    double getSampleRate() const noexcept { return sr; }

private:
    /** One-pole smoother; coefficient is set from a time constant in ms. */
    struct Smoother
    {
        void setTimeConstant (double sampleRate, float ms) noexcept
        {
            const double samples = (ms * 0.001) * sampleRate;
            coeff = samples > 0.0 ? static_cast<float> (1.0 - std::exp (-1.0 / samples)) : 1.0f;
        }

        void snapTo (float v) noexcept { current = target = v; }
        void setTarget (float v) noexcept { target = v; }

        float next() noexcept
        {
            current += coeff * (target - current);
            return current;
        }

        float coeff   = 1.0f;
        float current = 0.0f;
        float target  = 0.0f;
    };

    float msToSamples (float ms) const noexcept
    {
        return static_cast<float> (ms * 0.001 * sr);
    }

    Parameters params;

    DelayLine                     delayL, delayR;
    std::array<Lfo, numVoices>    lfos;

    Smoother smoothDelay, smoothDepth, smoothStereo, smoothMix;

    double sr          = 44100.0;
    bool   isPrepared  = false;
};

} // namespace chorus
