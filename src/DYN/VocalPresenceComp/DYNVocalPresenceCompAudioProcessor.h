#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"

class DYNVocalPresenceCompAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    DYNVocalPresenceCompAudioProcessor();
    ~DYNVocalPresenceCompAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNVocalPresenceComp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    struct EnvelopeFollower
    {
        void setSampleRate (double newRate)
        {
            sampleRate = newRate;
            updateCoefficients();
        }

        void setTimes (float attackMs, float releaseMs)
        {
            attack = juce::jmax (0.1f, attackMs);
            release = juce::jmax (1.0f, releaseMs);
            updateCoefficients();
        }

        float process (float input)
        {
            const float level = std::abs (input);
            const float coeff = level > state ? attackCoeff : releaseCoeff;
            state = coeff * state + (1.0f - coeff) * level;
            return state;
        }

        void reset() { state = 0.0f; }

    private:
        void updateCoefficients()
        {
            auto calc = [this](float timeMs)
            {
                const auto samples = static_cast<float> (sampleRate) * juce::jmax (0.001f, timeMs * 0.001f);
                return samples > 0.0f ? std::exp (-1.0f / samples) : 0.0f;
            };
            attackCoeff  = calc (attack);
            releaseCoeff = calc (release);
        }

        double sampleRate = 44100.0;
        float attack = 5.0f;
        float release = 120.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float state = 0.0f;
    };

    std::vector<EnvelopeFollower> presenceFollowers;
    std::vector<float> presenceGainSmoothers;
    std::vector<juce::dsp::IIR::Filter<float>> presenceFilters;
    std::vector<juce::dsp::IIR::Filter<float>> airFilters;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updatePresenceFilters (float freq, float q);
    void updateAirFilters (float airGainDb);
    float computePresenceGainDb (float levelDb, float thresholdDb, float rangeDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNVocalPresenceCompAudioProcessor)
};

class DYNVocalPresenceCompAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNVocalPresenceCompAudioProcessorEditor (DYNVocalPresenceCompAudioProcessor&);
    ~DYNVocalPresenceCompAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNVocalPresenceCompAudioProcessor& processorRef;

    juce::Slider presenceFreqSlider;
    juce::Slider presenceQSlider;
    juce::Slider presenceThreshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider airGainSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNVocalPresenceCompAudioProcessorEditor)
};
