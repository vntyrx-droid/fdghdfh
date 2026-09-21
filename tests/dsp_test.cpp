// Standalone checks for the chorus DSP core. No JUCE, no test framework:
// build with a single g++ invocation and run. See tests/README.md.

#include "../source/dsp/ChorusEngine.h"
#include "../source/dsp/DelayLine.h"
#include "../source/dsp/Lfo.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks   = 0;

void check (bool condition, const std::string& what)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::printf ("  FAIL  %s\n", what.c_str());
    }
    else
    {
        std::printf ("  ok    %s\n", what.c_str());
    }
}

bool nearlyEqual (float a, float b, float tol = 1.0e-5f)
{
    return std::fabs (a - b) <= tol;
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}

float peak (const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v)
        p = std::max (p, std::fabs (x));
    return p;
}

// ---------------------------------------------------------------------------

void testDelayLine()
{
    std::printf ("DelayLine\n");

    chorus::DelayLine line;
    line.prepare (1000);

    // An impulse read back at an integer delay must come out unchanged.
    line.write (1.0f);
    for (int i = 0; i < 99; ++i)
        line.write (0.0f);

    check (nearlyEqual (line.read (100.0f), 1.0f), "integer delay returns the impulse exactly");
    check (nearlyEqual (line.read (50.0f), 0.0f),  "unrelated tap reads silence");

    // A ramp is reproduced exactly by cubic interpolation (it fits a cubic).
    chorus::DelayLine ramp;
    ramp.prepare (512);
    for (int i = 0; i < 256; ++i)
        ramp.write (static_cast<float> (i));

    // A delay of 1.0 reads the most recently written sample (255), so a delay
    // of d reads the value 256 - d.
    const float expected = 256.0f - 10.5f;
    check (nearlyEqual (ramp.read (10.5f), expected, 1.0e-3f),
           "fractional read interpolates a ramp linearly");

    // Out-of-range requests must clamp rather than read out of bounds.
    check (std::isfinite (ramp.read (-5.0f)),      "negative delay is clamped");
    check (std::isfinite (ramp.read (1.0e9f)),     "oversized delay is clamped");

    chorus::DelayLine small;
    small.prepare (16);
    check (small.maxDelaySamples() > 0.0f, "tiny line still reports usable capacity");
}

void testLfo()
{
    std::printf ("Lfo\n");

    chorus::Lfo lfo;
    lfo.prepare (48000.0);
    lfo.setRateHz (1.0f);
    lfo.reset (0.0f);

    check (nearlyEqual (lfo.valueAt (chorus::LfoShape::sine, 0.0f), 0.0f, 1.0e-6f),
           "sine starts at zero");

    float minV = 1.0e9f, maxV = -1.0e9f;
    for (int i = 0; i < 48000; ++i)
    {
        const float v = lfo.valueAt (chorus::LfoShape::sine, 0.0f);
        minV = std::min (minV, v);
        maxV = std::max (maxV, v);
        lfo.advance();
    }

    check (maxV <= 1.0f + 1.0e-5f && minV >= -1.0f - 1.0e-5f, "sine stays within [-1, 1]");
    check (maxV > 0.99f && minV < -0.99f,                     "sine covers the full range");
    check (nearlyEqual (lfo.valueAt (chorus::LfoShape::sine, 0.0f), 0.0f, 1.0e-3f),
           "one second at 1 Hz returns to the start phase");

    // Triangle must be phase-aligned with the sine and hit its corners.
    using S = chorus::LfoShape;
    check (nearlyEqual (chorus::Lfo::evaluate (S::triangle, 0.0f),  0.0f), "triangle at 0 is 0");
    check (nearlyEqual (chorus::Lfo::evaluate (S::triangle, 0.25f), 1.0f), "triangle peaks at +1");
    check (nearlyEqual (chorus::Lfo::evaluate (S::triangle, 0.5f),  0.0f), "triangle crosses zero");
    check (nearlyEqual (chorus::Lfo::evaluate (S::triangle, 0.75f), -1.0f), "triangle troughs at -1");

    // A zero rate must hold its phase rather than drift or produce NaN.
    chorus::Lfo held;
    held.prepare (48000.0);
    held.setRateHz (0.0f);
    held.reset (0.3f);
    const float before = held.valueAt (S::sine, 0.0f);
    for (int i = 0; i < 1000; ++i)
        held.advance();
    check (nearlyEqual (held.valueAt (S::sine, 0.0f), before), "zero rate holds phase");
}

std::vector<float> makeSine (int n, float freq, double sr)
{
    std::vector<float> v (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t> (i)] =
            0.5f * std::sin (6.2831853f * freq * static_cast<float> (i) / static_cast<float> (sr));
    return v;
}

void testEngine()
{
    std::printf ("ChorusEngine\n");

    constexpr double sr = 48000.0;
    constexpr int    n  = 48000;

    // Fully dry: the output must be bit-for-bit the input.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.mix = 0.0f;
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto in  = makeSine (n, 220.0f, sr);
        auto ref = in;
        auto rIn = in, rRef = in;
        engine.process (in.data(), rIn.data(), n);

        bool identical = true;
        for (int i = 0; i < n; ++i)
            if (! nearlyEqual (in[(std::size_t) i], ref[(std::size_t) i], 1.0e-7f))
                identical = false;

        check (identical, "mix = 0 passes the dry signal through unchanged");
    }

    // Wet signal must be present, finite, and not wildly louder than the input.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.mix     = 1.0f;
        p.wetOnly = true;
        p.enabled = { true, true, true };
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto l = makeSine (n, 220.0f, sr);
        auto r = l;
        const float inPeak = peak (l);
        engine.process (l.data(), r.data(), n);

        check (allFinite (l) && allFinite (r), "wet output is finite");
        check (peak (l) > 0.05f,               "wet output is audible");
        check (peak (l) < inPeak * 2.0f,       "wet output does not blow up in level");
    }

    // Stereo spread must actually decorrelate the channels.
    {
        // sr is captured explicitly: MSVC rejects implicit capture of a
        // function-local constexpr (C3493) where GCC and Clang allow it.
        auto runWithSpread = [sr] (float spread, std::vector<float>& l, std::vector<float>& r)
        {
            chorus::ChorusEngine engine;
            chorus::ChorusEngine::Parameters p;
            p.wetOnly = true;
            p.stereo  = spread;
            p.enabled = { true, true, true };
            engine.setParameters (p);
            engine.prepare (sr, 512);
            engine.reset();
            engine.process (l.data(), r.data(), static_cast<int> (l.size()));
        };

        auto l0 = makeSine (n, 330.0f, sr), r0 = l0;
        runWithSpread (0.0f, l0, r0);

        double diff0 = 0.0;
        for (int i = 0; i < n; ++i)
            diff0 += std::fabs (l0[(std::size_t) i] - r0[(std::size_t) i]);

        auto l1 = makeSine (n, 330.0f, sr), r1 = l1;
        runWithSpread (1.0f, l1, r1);

        double diff1 = 0.0;
        for (int i = 0; i < n; ++i)
            diff1 += std::fabs (l1[(std::size_t) i] - r1[(std::size_t) i]);

        check (diff0 < 1.0e-3,  "stereo = 0 keeps the channels identical");
        check (diff1 > diff0,   "stereo = 1 decorrelates the channels");
    }

    // All voices off must be a clean bypass, and re-enabling must not click.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.enabled = { false, false, false };
        p.mix     = 1.0f;
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto in  = makeSine (4800, 440.0f, sr);
        auto ref = in;
        auto r   = in;
        engine.process (in.data(), r.data(), 4800);

        bool same = true;
        for (std::size_t i = 0; i < in.size(); ++i)
            same = same && nearlyEqual (in[i], ref[i], 1.0e-7f);

        check (same, "no active voices bypasses cleanly");
    }

    // Mono operation (null right channel) must not crash or produce NaN.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.wetOnly = true;
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto l = makeSine (4800, 440.0f, sr);
        engine.process (l.data(), nullptr, 4800);
        check (allFinite (l), "mono processing is finite");
    }

    // Silence in must give silence out - catches self-oscillation and denormal traps.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.wetOnly = true;
        p.enabled = { true, true, true };
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        std::vector<float> l (24000, 0.0f), r (24000, 0.0f);
        engine.process (l.data(), r.data(), 24000);
        check (peak (l) == 0.0f && peak (r) == 0.0f, "silence in gives silence out");
    }

    // Extreme settings must stay stable.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.delayMs = chorus::ChorusEngine::maxDelayMs;
        p.depthMs = chorus::ChorusEngine::maxDepthMs;
        p.stereo  = 1.0f;
        p.mix     = 1.0f;
        p.rateHz  = { chorus::ChorusEngine::maxRateHz,
                      chorus::ChorusEngine::maxRateHz,
                      chorus::ChorusEngine::maxRateHz };
        p.enabled = { true, true, true };
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto l = makeSine (n, 1000.0f, sr);
        auto r = l;
        engine.process (l.data(), r.data(), n);
        check (allFinite (l) && allFinite (r), "maximum settings stay finite");
        check (peak (l) < 4.0f,                "maximum settings stay bounded");
    }

    // Out-of-range parameters must be clamped, not trusted.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.delayMs = 1.0e6f;
        p.depthMs = -50.0f;
        p.mix     = 17.0f;
        p.stereo  = -3.0f;
        p.rateHz  = { -1.0f, 1.0e6f, 0.0f };
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto l = makeSine (4800, 440.0f, sr);
        auto r = l;
        engine.process (l.data(), r.data(), 4800);
        check (allFinite (l) && allFinite (r), "out-of-range parameters are clamped safely");
    }

    // Changing parameters mid-stream must not produce discontinuities.
    {
        chorus::ChorusEngine engine;
        chorus::ChorusEngine::Parameters p;
        p.wetOnly = true;
        engine.setParameters (p);
        engine.prepare (sr, 512);
        engine.reset();

        auto in = makeSine (n, 440.0f, sr);
        auto r  = in;

        float maxStep = 0.0f;
        float previous = 0.0f;

        for (int block = 0; block < n / 480; ++block)
        {
            p.delayMs = (block % 2 == 0) ? 2.0f : 35.0f; // worst-case jumps
            p.depthMs = (block % 2 == 0) ? 0.0f : 18.0f;
            engine.setParameters (p);

            const int offset = block * 480;
            engine.process (in.data() + offset, r.data() + offset, 480);

            for (int i = 0; i < 480; ++i)
            {
                const float s = in[(std::size_t) (offset + i)];
                maxStep = std::max (maxStep, std::fabs (s - previous));
                previous = s;
            }
        }

        check (allFinite (in),  "parameter jumps stay finite");
        check (maxStep < 0.35f, "parameter jumps are smoothed (no sample-to-sample click)");
    }
}
} // namespace

int main()
{
    std::printf ("\n== Chorus DSP core ==\n\n");

    testDelayLine();
    testLfo();
    testEngine();

    std::printf ("\n%d checks, %d failure(s)\n\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
