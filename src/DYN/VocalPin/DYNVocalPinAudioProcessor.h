#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include "../../ui/GoodluckLookAndFeel.h"
#include <array>
#include <vector>

class DYNVocalPinAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    DYNVocalPinAudioProcessor();
    ~DYNVocalPinAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DYNVocalPin"; }
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
    juce::AudioProcessorValueTreeState apvts;
    struct EnvelopeFollower
    {
        void setSampleRate (double newSampleRate)
        {
            sampleRate = newSampleRate;
            updateCoefficients();
        }

        void setTimes (float attackMs, float releaseMs)
        {
            attack = juce::jmax (0.1f, attackMs);
            release = juce::jmax (0.1f, releaseMs);
            updateCoefficients();
        }

        float process (float input)
        {
            const auto level = std::abs (input);
            const auto coeff = level > state ? attackCoeff : releaseCoeff;
            state = coeff * state + (1.0f - coeff) * level;
            return state;
        }

        void reset() { state = 0.0f; }

    private:
        void updateCoefficients()
        {
            auto calc = [this](float timeMs)
            {
                const auto samples = static_cast<float> (sampleRate) * juce::jmax (0.001f, timeMs * 0.001f);
                return samples > 0.0f ? std::exp (-1.0f / samples) : 0.0f;
            };

            attackCoeff  = calc (attack);
            releaseCoeff = calc (release);
        }

        double sampleRate = 44100.0;
        float attack = 5.0f;
        float release = 50.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float state = 0.0f;
    };

    std::vector<EnvelopeFollower> compFollowers;
    std::vector<EnvelopeFollower> deEssFollowers;
    std::vector<juce::dsp::IIR::Filter<float>> deEssFilters;
    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    int currentPreset = 0;

    void ensureStateSize (int numChannels);
    void updateDeEssFilters (float freq);
    float computeGain (float levelDb, float threshDb, float ratio) const;
    void applyPreset (int index);

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> params;
    };

    static const std::array<Preset, 3> presetBank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNVocalPinAudioProcessor)
};

class DYNVocalPinAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DYNVocalPinAudioProcessorEditor (DYNVocalPinAudioProcessor&);
    ~DYNVocalPinAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DYNVocalPinAudioProcessor& processorRef;

    juce::Colour accentColour;
    gls::ui::GoodluckLookAndFeel lookAndFeel;
    gls::ui::GoodluckHeader headerComponent;
    gls::ui::GoodluckFooter footerComponent;

    juce::Slider threshSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider deEssFreqSlider;
    juce::Slider deEssAmountSlider;
    juce::Slider mixSlider;
    juce::Slider inputTrimSlider;
    juce::Slider outputTrimSlider;
    juce::ToggleButton bypassButton { "Soft Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void initSlider (juce::Slider& slider, const juce::String& label, bool macro = false);
    void initToggle (juce::ToggleButton& toggle);
    void layoutLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYNVocalPinAudioProcessorEditor)
};
