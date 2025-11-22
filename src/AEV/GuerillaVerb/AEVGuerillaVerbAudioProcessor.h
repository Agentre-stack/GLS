#pragma once

#include <JuceHeader.h>
#include "../../DualPrecisionAudioProcessor.h"
#include <array>

class AEVGuerillaVerbAudioProcessor : public DualPrecisionAudioProcessor
{
public:
    AEVGuerillaVerbAudioProcessor();
    ~AEVGuerillaVerbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AEVGuerillaVerb"; }
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
    static constexpr int maxDelaySamples = 192000;

    struct Diffuser
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line { maxDelaySamples };
        float feedback = 0.0f;
    };

    std::vector<std::array<Diffuser, 4>> diffusers;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> preDelayLines
        { juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> (maxDelaySamples),
          juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> (maxDelaySamples) };
    juce::dsp::Reverb reverb;
    juce::dsp::IIR::Filter<float> hpfFilters[2];
    juce::dsp::IIR::Filter<float> lpfFilters[2];

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> workBuffer;
    juce::AudioBuffer<float> diffusionBuffer;
    juce::AudioBuffer<float> preDelaySnapshot;

    double currentSampleRate = 44100.0;
    juce::uint32 lastBlockSize = 512;
    std::array<float, 2> modulationPhase { 0.0f, 0.5f };

    void ensureStateSize (int numChannels, int numSamples);
    void updateFilters (float hpf, float lpf);
    void updateReverbParameters (float size, float decay, float density, float damping, float width, float erLevel);
    float processDiffusion (int channel, float input, float density, float damping, float irBlend);
    void applyWidth (juce::AudioBuffer<float>& buffer, float widthAmount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AEVGuerillaVerbAudioProcessor)
};

class AEVGuerillaVerbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AEVGuerillaVerbAudioProcessorEditor (AEVGuerillaVerbAudioProcessor&);
    ~AEVGuerillaVerbAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AEVGuerillaVerbAudioProcessor& processorRef;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;

    void addSlider (const juce::String& paramId, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AEVGuerillaVerbAudioProcessorEditor)
};
