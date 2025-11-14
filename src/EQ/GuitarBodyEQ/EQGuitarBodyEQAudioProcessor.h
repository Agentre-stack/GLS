#pragma once

#include <JuceHeader.h>

class EQGuitarBodyEQAudioProcessor : public juce::AudioProcessor
{
public:
    EQGuitarBodyEQAudioProcessor();
    ~EQGuitarBodyEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQGuitarBodyEQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    std::vector<juce::dsp::IIR::Filter<float>> bodyFilters;
    std::vector<juce::dsp::IIR::Filter<float>> mudFilters;
    std::vector<juce::dsp::IIR::Filter<float>> pickFilters;
    std::vector<juce::dsp::IIR::Filter<float>> airFilters;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureFilterState (int numChannels);
    void updateFilters (float bodyFreq, float bodyGain,
                        float mudCutFreq, float pickGain, float airGain);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQGuitarBodyEQAudioProcessor)
};

class EQGuitarBodyEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EQGuitarBodyEQAudioProcessorEditor (EQGuitarBodyEQAudioProcessor&);
    ~EQGuitarBodyEQAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EQGuitarBodyEQAudioProcessor& processorRef;

    juce::Slider bodyFreqSlider;
    juce::Slider bodyGainSlider;
    juce::Slider mudCutSlider;
    juce::Slider pickAttackSlider;
    juce::Slider airLiftSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQGuitarBodyEQAudioProcessorEditor)
};
