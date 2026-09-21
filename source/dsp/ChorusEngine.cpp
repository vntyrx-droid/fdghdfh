#include "ChorusEngine.h"

#include <algorithm>

namespace chorus
{

void ChorusEngine::prepare (double sampleRate, int /*maximumBlockSize*/)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Enough room for the deepest sweep the parameter ranges allow, plus a
    // little headroom for the interpolator.
    const int maxSamples = static_cast<int> (msToSamples (maxDelayMs + maxDepthMs)) + 8;

    delayL.prepare (maxSamples);
    delayR.prepare (maxSamples);

    for (int v = 0; v < numVoices; ++v)
    {
        lfos[v].prepare (sr);
        lfos[v].setRateHz (params.rateHz[v]);
        // Spread the voices evenly around the cycle so they never stack up.
        lfos[v].reset (static_cast<float> (v) / static_cast<float> (numVoices));
    }

    for (auto* s : { &smoothDelay, &smoothDepth, &smoothStereo, &smoothMix })
        s->setTimeConstant (sr, 20.0f);

    smoothDelay .snapTo (params.delayMs);
    smoothDepth .snapTo (params.depthMs);
    smoothStereo.snapTo (params.stereo);
    smoothMix   .snapTo (params.mix);

    isPrepared = true;
}

void ChorusEngine::reset()
{
    delayL.reset();
    delayR.reset();

    for (int v = 0; v < numVoices; ++v)
        lfos[v].reset (static_cast<float> (v) / static_cast<float> (numVoices));

    smoothDelay .snapTo (params.delayMs);
    smoothDepth .snapTo (params.depthMs);
    smoothStereo.snapTo (params.stereo);
    smoothMix   .snapTo (params.mix);
}

void ChorusEngine::setParameters (const Parameters& newParams)
{
    params = newParams;

    params.delayMs = std::clamp (params.delayMs, 0.5f, maxDelayMs);
    params.depthMs = std::clamp (params.depthMs, 0.0f, maxDepthMs);
    params.stereo  = std::clamp (params.stereo,  0.0f, 1.0f);
    params.mix     = std::clamp (params.mix,     0.0f, 1.0f);

    for (int v = 0; v < numVoices; ++v)
    {
        params.rateHz[v] = std::clamp (params.rateHz[v], 0.0f, maxRateHz);
        lfos[v].setRateHz (params.rateHz[v]);
    }

    smoothDelay .setTarget (params.delayMs);
    smoothDepth .setTarget (params.depthMs);
    smoothStereo.setTarget (params.stereo);
    smoothMix   .setTarget (params.mix);
}

void ChorusEngine::process (float* left, float* right, int numSamples)
{
    if (! isPrepared || left == nullptr || numSamples <= 0)
        return;

    const bool stereoMode = (right != nullptr);

    int activeVoices = 0;
    for (int v = 0; v < numVoices; ++v)
        if (params.enabled[v])
            ++activeVoices;

    // Nothing modulating: pass the dry signal through untouched, but keep the
    // delay lines fed so re-enabling a voice does not read stale silence.
    if (activeVoices == 0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            delayL.write (left[i]);
            if (stereoMode)
                delayR.write (right[i]);
        }
        return;
    }

    const float voiceScale = 1.0f / static_cast<float> (activeVoices);

    for (int i = 0; i < numSamples; ++i)
    {
        const float baseSamples  = msToSamples (smoothDelay.next());
        const float depthSamples = msToSamples (smoothDepth.next());
        const float stereoAmount = smoothStereo.next();
        const float mix          = smoothMix.next();

        // Half a cycle at full stereo puts the channels in opposition.
        const float stereoOffset = stereoAmount * 0.5f;

        const float dryL = left[i];
        const float dryR = stereoMode ? right[i] : dryL;

        delayL.write (dryL);
        if (stereoMode)
            delayR.write (dryR);

        float wetL = 0.0f;
        float wetR = 0.0f;

        for (int v = 0; v < numVoices; ++v)
        {
            lfos[v].advance();

            if (! params.enabled[v])
                continue;

            const float voicePhase = static_cast<float> (v) / static_cast<float> (numVoices);

            // Sweep upward from the base delay so the read pointer can never
            // cross the write head, whatever the depth setting.
            const float modL = lfos[v].valueAt (params.shape, voicePhase);
            wetL += delayL.read (baseSamples + depthSamples * 0.5f * (1.0f + modL));

            if (stereoMode)
            {
                const float modR = lfos[v].valueAt (params.shape, voicePhase + stereoOffset);
                wetR += delayR.read (baseSamples + depthSamples * 0.5f * (1.0f + modR));
            }
        }

        wetL *= voiceScale;
        wetR *= voiceScale;

        if (params.wetOnly)
        {
            left[i] = wetL;
            if (stereoMode)
                right[i] = wetR;
        }
        else
        {
            left[i] = dryL * (1.0f - mix) + wetL * mix;
            if (stereoMode)
                right[i] = dryR * (1.0f - mix) + wetR * mix;
        }
    }
}

} // namespace chorus
