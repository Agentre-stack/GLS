#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GLSXOverBusAudioProcessor : public DualPrecisionAudioProcessor
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
    const juce::String getProgramName (int index) override { return index == 0 ? juce::String (JucePlugin_Name " 01") : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;

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
    juce::AudioBuffer<float> originalBuffer;

    void prepareFilters (BandFilters& filters, int order, const juce::dsp::ProcessSpec& spec);
    void updateCoefficients (BandFilters& filters, float freq, bool isLow);
    void applyFilters (BandFilters& filters, juce::AudioBuffer<float>& buffer, bool isLow);
    void ensureBufferSize (int channels, int samples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSXOverBusAudioProcessor)
};

class XOverVisual;

class GLSXOverBusAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLSXOverBusAudioProcessorEditor (GLSXOverBusAudioProcessor&);
    ~GLSXOverBusAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GLSXOverBusAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<XOverVisual> centerVisual;

    juce::Slider split1Slider;
    juce::Slider split2Slider;
    juce::Slider slopeSlider;
    juce::ToggleButton band1SoloButton { "Low" };
    juce::ToggleButton band2SoloButton { "Mid" };
    juce::ToggleButton band3SoloButton { "High" };
    juce::Slider inputTrimSlider;
    juce::Slider dryWetSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    struct LabeledSliderRef
    {
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
    };

    std::vector<std::unique_ptr<juce::Label>> sliderLabels;
    std::vector<LabeledSliderRef> labeledSliders;

    void configureSlider (juce::Slider& slider, const juce::String& labelText, bool isMacro, bool isLinear = false);
    void configureToggle (juce::ToggleButton& toggle, const juce::String& labelText);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLSXOverBusAudioProcessorEditor)
};
