#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Range  = juce::NormalisableRange<float>;
using FParam = juce::AudioParameterFloat;
using Attrs  = juce::AudioParameterFloatAttributes;

juce::String msText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
}
}

juce::AudioProcessorValueTreeState::ParameterLayout UbermenschAudioProcessor::createParameterLayout()
{
    using juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "engage", 1 }, "Engage", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "grab", 1 },   "Grab", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                              juce::StringArray { "Freeze", "Sustain", "Synth" }, 0));

    auto ms = [] (float v, int) { return msText (v); };

    layout.add (std::make_unique<FParam> (ParameterID { "attack", 1 }, "Attack",
        Range (5.0f, 5000.0f, 0.0f, 0.35f), 40.0f, Attrs().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<FParam> (ParameterID { "decay", 1 }, "Decay",
        Range (50.0f, 20000.0f, 0.0f, 0.35f), 4000.0f, Attrs().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<FParam> (ParameterID { "gliss", 1 }, "Gliss",
        Range (0.0f, 4000.0f, 0.0f, 0.4f), 200.0f, Attrs().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<FParam> (ParameterID { "sub", 1 }, "Sub",
        Range (0.0f, 1.0f), 0.5f, Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));
    layout.add (std::make_unique<FParam> (ParameterID { "oct", 1 }, "Oct",
        Range (0.0f, 1.0f), 0.3f, Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));
    layout.add (std::make_unique<FParam> (ParameterID { "tone", 1 }, "Tone",
        Range (200.0f, 20000.0f, 0.0f, 0.35f), 12000.0f,
        Attrs().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; })));
    layout.add (std::make_unique<FParam> (ParameterID { "width", 1 }, "Width",
        Range (0.0f, 1.0f), 0.6f, Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));
    layout.add (std::make_unique<FParam> (ParameterID { "mix", 1 }, "Mix",
        Range (0.0f, 1.0f), 0.5f, Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));
    layout.add (std::make_unique<FParam> (ParameterID { "level", 1 }, "Level",
        Range (-24.0f, 12.0f, 0.1f), 0.0f, Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));
    return layout;
}

UbermenschAudioProcessor::UbermenschAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    pEngage = apvts.getRawParameterValue ("engage");
    pGrab   = apvts.getRawParameterValue ("grab");
    pMode   = apvts.getRawParameterValue ("mode");
    pAttack = apvts.getRawParameterValue ("attack");
    pDecay  = apvts.getRawParameterValue ("decay");
    pGliss  = apvts.getRawParameterValue ("gliss");
    pSub    = apvts.getRawParameterValue ("sub");
    pOct    = apvts.getRawParameterValue ("oct");
    pTone   = apvts.getRawParameterValue ("tone");
    pWidth  = apvts.getRawParameterValue ("width");
    pMix    = apvts.getRawParameterValue ("mix");
    pLevel  = apvts.getRawParameterValue ("level");
}

bool UbermenschAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == layouts.getMainInputChannelSet();
}

void UbermenschAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    const auto scratch = (size_t) juce::jmax (samplesPerBlock, 512);
    wetL.assign (scratch, 0.0f);
    wetR.assign (scratch, 0.0f);
    drySmooth.reset (sampleRate, 0.03);
    wetSmooth.reset (sampleRate, 0.03);
    drySmooth.setCurrentAndTargetValue (1.0f);
    wetSmooth.setCurrentAndTargetValue (1.0f);
    pedalDown = lastGrab = false;
}

void UbermenschAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (2, buffer.getNumChannels());

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numChannels == 0) return;

    // MIDI sustain pedal (CC64) also engages the effect: handy for a footswitch.
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isControllerOfType (64)) pedalDown = msg.getControllerValue() >= 64;
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) pedalDown = false;
    }

    ubermensch::Params p;
    p.mode     = (ubermensch::Mode) juce::jlimit (0, 2, juce::roundToInt (pMode->load()));
    p.attackMs = pAttack->load();
    p.decayMs  = pDecay->load();
    p.glissMs  = pGliss->load();
    p.subLevel = pSub->load();
    p.octLevel = pOct->load();
    p.toneHz   = pTone->load();
    p.width    = pWidth->load();
    p.levelDb  = pLevel->load();
    engine.setParams (p);

    const bool engaged = pEngage->load() > 0.5f || pedalDown;
    engine.setEngaged (engaged);
    const bool grabNow = pGrab->load() > 0.5f;
    if (grabNow && ! lastGrab && engaged) engine.grab();
    lastGrab = grabNow;

    // Blend: 0 = dry only, 0.5 = both at unity, 1 = wet only
    const float mix = pMix->load();
    drySmooth.setTargetValue (juce::jmin (1.0f, 2.0f * (1.0f - mix)));
    wetSmooth.setTargetValue (juce::jmin (1.0f, 2.0f * mix));

    float* l = buffer.getWritePointer (0);
    float* r = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    const int chunkMax = (int) wetL.size();

    for (int start = 0; start < numSamples; start += chunkMax)
    {
        const int n = juce::jmin (chunkMax, numSamples - start);
        engine.process (l + start, r != nullptr ? r + start : l + start, wetL.data(), wetR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const float d = drySmooth.getNextValue(), w = wetSmooth.getNextValue();
            l[start + i] = l[start + i] * d + wetL[(size_t) i] * w;
            if (r != nullptr) r[start + i] = r[start + i] * d + wetR[(size_t) i] * w;
        }
    }
    envelopeMeter.store (engine.getEnvelope());
}

juce::AudioProcessorEditor* UbermenschAudioProcessor::createEditor()
{
    return new UbermenschAudioProcessorEditor (*this);
}

void UbermenschAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void UbermenschAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UbermenschAudioProcessor();
}
