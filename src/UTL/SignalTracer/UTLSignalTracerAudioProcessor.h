#pragma once

#include <JuceHeader.h>
#include <mutex>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class UTLSignalTracerAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    struct TapMetrics
    {
        float peak = 0.0f;
        float rms  = 0.0f;
        float correlation = 0.0f;
    };

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
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String ("UTL Signal Tracer 01") : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void copyTapMetrics (std::array<TapMetrics, 4>& dest) const;
    void copyTapBuffer (int tapIndex, juce::AudioBuffer<float>& dest) const;
    float getPhaseCorrelation() const noexcept { return phaseCorrelation.load(); }
    juce::String getTapLabel (int index) const;
    void setTapLabel (int index, const juce::String& text);
    void storeTapPreset (int slot);
    void loadTapPreset (int slot);

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioBuffer<float> inputSnapshot;
    juce::AudioBuffer<float> sideSnapshot;
    juce::AudioBuffer<float> postSnapshot;
    std::array<TapMetrics, 4> tapMetrics {};
    std::array<float, 4> rmsAverages {};
    std::array<float, 4> peakHoldValues {};
    std::array<juce::String, 4> tapLabels { "Input", "Pre", "Post", "Side" };
    std::array<std::array<juce::String, 4>, 3> tapLabelPresets {};
    std::atomic<float> phaseCorrelation { 0.0f };
    mutable std::mutex metricsMutex;

    void computeTapMetrics (int tapIndex, const juce::AudioBuffer<float>& buffer,
                            float smoothingCoeff, bool holdPeaks);
    void updateTapLabelsFromState();
    void updatePhaseCorrelation (const juce::AudioBuffer<float>& buffer);
    const juce::AudioBuffer<float>& getTapBufferForCopy (int tapIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLSignalTracerAudioProcessor)
};

//==============================================================================
class UTLSignalTracerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UTLSignalTracerAudioProcessorEditor (UTLSignalTracerAudioProcessor&);
    ~UTLSignalTracerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UTLSignalTracerAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;
    std::unique_ptr<juce::Component> visualComponent;

    juce::ComboBox tapBox;
    juce::ComboBox phaseViewBox;
    juce::ToggleButton peakHoldButton { "Peak Hold" };
    juce::Slider rmsWindowSlider;
    juce::ComboBox routingModeBox;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };
    juce::TextEditor tapLabelEditor;
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Store Slot" };

    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAttachment> tapAttachment;
    std::unique_ptr<ComboAttachment> phaseAttachment;
    std::unique_ptr<ButtonAttachment> peakHoldAttachment;
    std::unique_ptr<SliderAttachment> rmsAttachment;
    std::unique_ptr<ComboAttachment> routingAttachment;
    std::unique_ptr<SliderAttachment> inputTrimAttachment;
    std::unique_ptr<SliderAttachment> outputTrimAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    void refreshTapLabels();
    void configureSlider (juce::Slider& slider, const juce::String& name, bool isMacro);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UTLSignalTracerAudioProcessorEditor)
};
