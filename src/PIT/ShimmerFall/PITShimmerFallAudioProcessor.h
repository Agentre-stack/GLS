#pragma once

#include <JuceHeader.h>

class PITShimmerFallAudioProcessor : public juce::AudioProcessor
{
public:
    PITShimmerFallAudioProcessor();
    ~PITShimmerFallAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "PITShimmerFall"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    juce::dsp::Reverb reverb;
    juce::dsp::ProcessSpec currentSpec { 44100.0, 512, 2 };
    juce::AudioBuffer<float> shimmerBuffer;

    void updateReverbParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITShimmerFallAudioProcessor)
};

//==============================================================================
class PITShimmerFallAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PITShimmerFallAudioProcessorEditor (PITShimmerFallAudioProcessor&);
    ~PITShimmerFallAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PITShimmerFallAudioProcessor& processorRef;

    juce::Slider pitchIntervalSlider;
    juce::Slider feedbackSlider;
    juce::Slider dampingSlider;
    juce::Slider timeSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PITShimmerFallAudioProcessorEditor)
};
