#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include "UbermenschEngine.h"

class UbermenschAudioProcessor : public juce::AudioProcessor
{
public:
    UbermenschAudioProcessor();
    ~UbermenschAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> envelopeMeter { 0.0f };   // for the editor's glow

private:
    ubermensch::Engine engine;
    std::vector<float> wetL, wetR;

    std::atomic<float> *pEngage {}, *pGrab {}, *pMode {}, *pAttack {}, *pDecay {}, *pGliss {},
                       *pSub {}, *pOct {}, *pTone {}, *pWidth {}, *pMix {}, *pLevel {};

    juce::SmoothedValue<float> drySmooth, wetSmooth;
    bool pedalDown = false, lastGrab = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UbermenschAudioProcessor)
};
