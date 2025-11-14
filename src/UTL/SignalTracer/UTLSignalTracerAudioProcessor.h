#pragma once

#include <JuceHeader.h>

class UTLSignalTracerAudioProcessor : public juce::AudioProcessor
{
public:
    UTLSignalTracerAudioProcessor();
    ~UTLSignalTracerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UTLSignalTracer"; }
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLSignalTracerAudioProcessor)
};

//==============================================================================
class UTLSignalTracerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLSignalTracerAudioProcessorEditor (UTLSignalTracerAudioProcessor&);
    ~UTLSignalTracerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLSignalTracerAudioProcessor& processorRef;

    juce::ComboBox tapBox;
    juce::ComboBox phaseViewBox;
    juce::ToggleButton peakHoldButton { "Peak Hold" };
    juce::Slider rmsWindowSlider;
    juce::ComboBox routingModeBox;

    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAttachment> tapAttachment;
    std::unique_ptr<ComboAttachment> phaseAttachment;
    std::unique_ptr<ButtonAttachment> peakHoldAttachment;
    std::unique_ptr<SliderAttachment> rmsAttachment;
    std::unique_ptr<ComboAttachment> routingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLSignalTracerAudioProcessorEditor)
};
