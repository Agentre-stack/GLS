#pragma once

#include <JuceHeader.h>

class MDLFlangerJetAudioProcessor : public juce::AudioProcessor
{
public:
    MDLFlangerJetAudioProcessor();
    ~MDLFlangerJetAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLFlangerJet"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

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

    struct FlangerLine
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 48000 };
        float lfoPhase = 0.0f;
    };

    std::vector<FlangerLine> lines;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

    void ensureStateSize (int numChannels);
    void updateDelayBounds();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLFlangerJetAudioProcessor)
};

class MDLFlangerJetAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLFlangerJetAudioProcessorEditor (MDLFlangerJetAudioProcessor&);
    ~MDLFlangerJetAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLFlangerJetAudioProcessor& processorRef;

    juce::Slider delaySlider;
    juce::Slider depthSlider;
    juce::Slider rateSlider;
    juce::Slider feedbackSlider;
    juce::Slider manualSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLFlangerJetAudioProcessorEditor)
};
