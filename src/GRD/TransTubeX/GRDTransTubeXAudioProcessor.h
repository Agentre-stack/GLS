#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"

class GRDTransTubeXAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    GRDTransTubeXAudioProcessor();
    ~GRDTransTubeXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRDTransTubeX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct TransientTracker
    {
        void setSampleRate (double sr)
        {
            sampleRate = sr;
            updateCoeffs();
        }

        void setTimes (float fastMs, float slowMs)
        {
            fastTime = juce::jmax (0.1f, fastMs);
            slowTime = juce::jmax (fastTime, slowMs);
            updateCoeffs();
        }

        float process (float x)
        {
            const float level = std::abs (x);
            fastEnv = fastCoeff * fastEnv + (1.0f - fastCoeff) * level;
            slowEnv = slowCoeff * slowEnv + (1.0f - slowCoeff) * level;
            return juce::jmax (0.0f, fastEnv - slowEnv);
        }

        void reset()
        {
            fastEnv = 0.0f;
            slowEnv = 0.0f;
        }

    private:
        void updateCoeffs()
        {
            auto toCoeff = [this](float ms)
            {
                const auto samples = sampleRate * ms * 0.001;
                return samples > 0.0 ? std::exp (-1.0 / samples) : 0.0;
            };

            fastCoeff = toCoeff (fastTime);
            slowCoeff = toCoeff (slowTime);
        }

        double sampleRate = 44100.0;
        float fastTime = 5.0f;
        float slowTime = 50.0f;
        float fastCoeff = 0.0f;
        float slowCoeff = 0.0f;
        float fastEnv = 0.0f;
        float slowEnv = 0.0f;
    };

    juce::AudioProcessorValueTreeState apvts;
    std::vector<TransientTracker> trackers;
    std::vector<juce::dsp::IIR::Filter<float>> toneFilters;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    int currentPreset = 0;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    void ensureStateSize (int numChannels, int numSamples);
    void applyPreset (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDTransTubeXAudioProcessor)
};

class GRDTransTubeXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GRDTransTubeXAudioProcessorEditor (GRDTransTubeXAudioProcessor&);
    ~GRDTransTubeXAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GRDTransTubeXAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider driveSlider;
    juce::Slider transSensSlider;
    juce::Slider attackBiasSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    void initSlider (juce::Slider& slider, const juce::String& label, bool macro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GRDTransTubeXAudioProcessorEditor)
};
