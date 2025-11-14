#pragma once

#include <JuceHeader.h>

class GLSXOverBusAudioProcessor : public juce::AudioProcessor
{
public:
    GLSXOverBusAudioProcessor();
    ~GLSXOverBusAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLSXOverBus"; }
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
    double currentSampleRate = 44100.0;

    struct BandFilters
    {
        std::vector<juce::dsp::LinkwitzRileyFilter<float>> lowFilters;
        std::vector<juce::dsp::LinkwitzRileyFilter<float>> highFilters;
    };

    BandFilters lowBand;
    BandFilters midBandLow;
    BandFilters midBandHigh;
    BandFilters highBand;

    juce::AudioBuffer<float> lowBuffer;
    juce::AudioBuffer<float> midBuffer;
    juce::AudioBuffer<float> highBuffer;

    void prepareFilters (BandFilters& filters, int order, const juce::dsp::ProcessSpec& spec);
    void updateCoefficients (BandFilters& filters, float freq, bool isLow);
    void applyFilters (BandFilters& filters, juce::AudioBuffer<float>& buffer, bool isLow);
    void ensureBufferSize (int channels, int samples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSXOverBusAudioProcessor)
};

class GLSXOverBusAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSXOverBusAudioProcessorEditor (GLSXOverBusAudioProcessor&);
    ~GLSXOverBusAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSXOverBusAudioProcessor& processorRef;

    juce::Slider split1Slider;
    juce::Slider split2Slider;
    juce::Slider slopeSlider;
    juce::ToggleButton band1SoloButton { "Band 1" };
    juce::ToggleButton band2SoloButton { "Band 2" };
    juce::ToggleButton band3SoloButton { "Band 3" };
    juce::Slider outputSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> split1Attachment;
    std::unique_ptr<SliderAttachment> split2Attachment;
    std::unique_ptr<SliderAttachment> slopeAttachment;
    std::unique_ptr<ButtonAttachment> band1Attachment;
    std::unique_ptr<ButtonAttachment> band2Attachment;
    std::unique_ptr<ButtonAttachment> band3Attachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSXOverBusAudioProcessorEditor)
};
