#pragma once

#include <JuceHeader.h>

class MDLWideTrackAudioProcessor : public juce::AudioProcessor
{
public:
    MDLWideTrackAudioProcessor();
    ~MDLWideTrackAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MDLWideTrack"; }
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

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> sumDiffBuffer;
    double currentSampleRate = 44100.0;
    juce::Random random;

    void applyMicroDelay (juce::AudioBuffer<float>& buffer, float spreadMs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLWideTrackAudioProcessor)
};

class MDLWideTrackAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MDLWideTrackAudioProcessorEditor (MDLWideTrackAudioProcessor&);
    ~MDLWideTrackAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MDLWideTrackAudioProcessor& processorRef;

    juce::Slider widthSlider;
    juce::Slider delaySpreadSlider;
    juce::Slider hfSlider;
    juce::Slider monoSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MDLWideTrackAudioProcessorEditor)
};
