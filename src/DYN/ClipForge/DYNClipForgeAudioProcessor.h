#pragma once

#include <JuceHeader.h>

class DYNClipForgeAudioProcessor : public juce::AudioProcessor
{
public:
    DYNClipForgeAudioProcessor();
    ~DYNClipForgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNClipForge"; }
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
    juce::dsp::IIR::Filter<float> preHpfFilter;
    juce::dsp::IIR::Filter<float> postToneFilter;
    double currentSampleRate = 44100.0;

    void updateFilters (float preHpfFreq, float postTone);
    static float softClip (float x, float knee);
    static float hardClip (float x, float ceiling);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNClipForgeAudioProcessor)
};

class DYNClipForgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNClipForgeAudioProcessorEditor (DYNClipForgeAudioProcessor&);
    ~DYNClipForgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNClipForgeAudioProcessor& processorRef;

    juce::Slider ceilingSlider;
    juce::Slider clipBlendSlider;
    juce::Slider kneeSlider;
    juce::Slider preHpfSlider;
    juce::Slider postToneSlider;
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNClipForgeAudioProcessorEditor)
};
