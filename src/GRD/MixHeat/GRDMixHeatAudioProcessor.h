#pragma once

#include <JuceHeader.h>

class GRDMixHeatAudioProcessor : public juce::AudioProcessor
{
public:
    GRDMixHeatAudioProcessor();
    ~GRDMixHeatAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDMixHeat"; }
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

    juce::dsp::IIR::Filter<float> toneFilter;
    double currentSampleRate = 44100.0;

    float driveToGain (float drive) const;
    float applySaturation (float sample, float drive, int mode) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDMixHeatAudioProcessor)
};

class GRDMixHeatAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDMixHeatAudioProcessorEditor (GRDMixHeatAudioProcessor&);
    ~GRDMixHeatAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDMixHeatAudioProcessor& processorRef;

    juce::ComboBox modeBox;
    juce::Slider driveSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> modeAttachment;

    void initSlider (juce::Slider&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDMixHeatAudioProcessorEditor)
};
