// Instantiates the real AudioProcessor and exercises the host-facing surface:
// parameter wiring, prepare/process, bus layouts and state round-tripping.
// The editor is not created here so that the test runs headless.

#include "../source/PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
int failures = 0;
int checks   = 0;

void check (bool condition, const std::string& what)
{
    ++checks;
    std::printf ("  %s  %s\n", condition ? "ok  " : "FAIL", what.c_str());
    if (! condition)
        ++failures;
}

bool bufferIsFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

void fillWithSine (juce::AudioBuffer<float>& b, float freq, double sr)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, 0.5f * std::sin (6.2831853f * freq * (float) i / (float) sr));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("\n== Chorus plugin wrapper ==\n\n");

    ChorusAudioProcessor proc;
    auto& apvts = proc.getValueTreeState();

    // Every ID the processor caches must actually resolve, or the audio thread
    // would dereference a null pointer on the first block.
    for (const char* id : { ids::delay, ids::depth, ids::stereo, ids::shape,
                            ids::mix, ids::wetOnly })
        check (apvts.getRawParameterValue (id) != nullptr,
               std::string ("parameter exists: ") + id);

    for (int v = 0; v < chorus::ChorusEngine::numVoices; ++v)
    {
        check (apvts.getRawParameterValue (ids::voiceRate (v)) != nullptr,
               ("parameter exists: " + ids::voiceRate (v)).toStdString());
        check (apvts.getRawParameterValue (ids::voiceEnabled (v)) != nullptr,
               ("parameter exists: " + ids::voiceEnabled (v)).toStdString());
    }

    check (proc.getParameters().size() == 12, "exposes the expected parameter count");

    // Stereo processing.
    constexpr double sr = 48000.0;
    proc.setPlayConfigDetails (2, 2, sr, 512);
    proc.prepareToPlay (sr, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    fillWithSine (buffer, 440.0f, sr);
    proc.processBlock (buffer, midi);
    check (bufferIsFinite (buffer), "stereo block produces finite output");
    check (buffer.getMagnitude (0, 512) > 0.0f, "stereo block produces signal");

    // A run of blocks must stay stable (catches state carried between blocks).
    bool stable = true;
    for (int i = 0; i < 200; ++i)
    {
        fillWithSine (buffer, 440.0f, sr);
        proc.processBlock (buffer, midi);
        stable = stable && bufferIsFinite (buffer) && buffer.getMagnitude (0, 512) < 8.0f;
    }
    check (stable, "200 consecutive blocks stay finite and bounded");

    // Mono must work too, since the bus layout advertises it.
    ChorusAudioProcessor monoProc;
    monoProc.setPlayConfigDetails (1, 1, sr, 256);
    monoProc.prepareToPlay (sr, 256);
    juce::AudioBuffer<float> monoBuffer (1, 256);
    fillWithSine (monoBuffer, 330.0f, sr);
    monoProc.processBlock (monoBuffer, midi);
    check (bufferIsFinite (monoBuffer), "mono block produces finite output");

    // An empty block must not crash.
    juce::AudioBuffer<float> emptyBuffer (2, 0);
    proc.processBlock (emptyBuffer, midi);
    check (true, "zero-length block is handled");

    // State round-trip: change a value, save, change again, restore.
    {
        auto* mixParam = apvts.getParameter (ids::mix);
        check (mixParam != nullptr, "mix parameter is addressable by the host");

        mixParam->setValueNotifyingHost (0.25f);
        const float saved = apvts.getRawParameterValue (ids::mix)->load();

        juce::MemoryBlock blob;
        proc.getStateInformation (blob);
        check (blob.getSize() > 0, "state serialises to a non-empty blob");

        mixParam->setValueNotifyingHost (0.9f);
        proc.setStateInformation (blob.getData(), (int) blob.getSize());

        const float restored = apvts.getRawParameterValue (ids::mix)->load();
        check (std::fabs (restored - saved) < 1.0e-3f, "state round-trips the mix value");
    }

    // Garbage state must be rejected rather than crash the host.
    {
        const char junk[] = "not a valid plugin state at all";
        proc.setStateInformation (junk, (int) sizeof (junk));
        check (true, "malformed state is rejected without crashing");
    }

    check (proc.getTailLengthSeconds() > 0.0, "reports a non-zero tail length");
    check (! proc.acceptsMidi() && ! proc.producesMidi(), "declares itself a pure audio effect");

    proc.releaseResources();

    std::printf ("\n%d checks, %d failure(s)\n\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
