#include "GLSChannelPilotAudioProcessor.h"

namespace
{
constexpr auto kStateId        = "CHANNEL_PILOT";
constexpr auto kParamInputTrim = "input_trim";
constexpr auto kParamHpf       = "hpf_freq";
constexpr auto kParamLpf       = "lpf_freq";
constexpr auto kParamPhase     = "phase";
constexpr auto kParamPan       = "pan";
constexpr auto kParamOutput    = "output_trim";
constexpr auto kParamSlope     = "filter_slope";
constexpr auto kParamAutoGain  = "auto_gain";
constexpr auto kParamBypass    = "ui_bypass";

class ChannelPilotHeroComponent : public juce::Component, private juce::Timer
{
public:
    ChannelPilotHeroComponent (GLSChannelPilotAudioProcessor& proc,
                               juce::AudioProcessorValueTreeState& stateRef,
                               juce::Colour accentColour)
        : processor (proc), state (stateRef), accent (accentColour)
    {
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 12.0f, 1.4f);

        auto headerArea = bounds.removeFromTop (28.0f);
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Filters & pan visual", headerArea.toNearestInt(), juce::Justification::centredLeft, 1);
        g.drawFittedText ("Auto gain", headerArea.toNearestInt(), juce::Justification::centredRight, 1);

        auto freqArea = bounds.removeFromTop (bounds.getHeight() * 0.55f).reduced (16.0f);
        drawFilters (g, freqArea);

        auto panArea = bounds.removeFromTop (bounds.getHeight() * 0.55f).reduced (16.0f, 10.0f);
        drawPan (g, panArea);

        auto gainArea = bounds.reduced (16.0f);
        drawAutoGain (g, gainArea);
    }

private:
    GLSChannelPilotAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    juce::Colour accent;
    float hpfFreq = 20.0f;
    float lpfFreq = 20000.0f;
    float pan = 0.0f;
    int slope = 0;
    float autoGain = 1.0f;

    static float logNorm (float value)
    {
        const float minF = 20.0f;
        const float maxF = 20000.0f;
        return juce::jlimit (0.0f, 1.0f, (std::log (value) - std::log (minF)) / (std::log (maxF) - std::log (minF)));
    }

    void drawFilters (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (area, 8.0f, 1.2f);

        const float hpfNorm = logNorm (hpfFreq);
        const float lpfNorm = logNorm (lpfFreq);
        auto hpfX = area.getX() + area.getWidth() * hpfNorm;
        auto lpfX = area.getX() + area.getWidth() * lpfNorm;

        g.setColour (accent.withMultipliedAlpha (0.8f));
        g.drawLine (hpfX, area.getY(), hpfX, area.getBottom(), 2.0f);
        g.drawLine (lpfX, area.getY(), lpfX, area.getBottom(), 2.0f);

        g.setFont (gls::ui::makeFont (12.0f));
        g.setColour (gls::ui::Colours::text());
        g.drawFittedText ("HPF " + juce::String ((int) hpfFreq) + " Hz", area.toNearestInt().withWidth ((int) area.getWidth() / 2),
                          juce::Justification::centredLeft, 1);
        g.drawFittedText ("LPF " + juce::String ((int) lpfFreq) + " Hz", area.toNearestInt().withTrimmedLeft ((int) area.getWidth() / 2),
                          juce::Justification::centredRight, 1);

        g.setColour (gls::ui::Colours::textSecondary());
        juce::String slopeText = slope == 0 ? "12 dB/oct" : "24 dB/oct";
        g.drawFittedText ("Slope: " + slopeText, area.toNearestInt().translated (0, (int) area.getHeight() - 18),
                          juce::Justification::centred, 1);
    }

    void drawPan (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (area, 8.0f, 1.2f);

        const float panNorm = juce::jmap (pan, -1.0f, 1.0f, 0.0f, 1.0f);
        auto needleX = area.getX() + area.getWidth() * panNorm;
        g.setColour (accent);
        g.drawLine (needleX, area.getY(), needleX, area.getBottom(), 3.0f);

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Pan", area.toNearestInt().removeFromTop (16), juce::Justification::centredLeft, 1);
        g.drawFittedText (juce::String (pan, 2), area.toNearestInt(), juce::Justification::centred, 1);
    }

    void drawAutoGain (juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto meter = area.withHeight (18.0f);
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (meter, 6.0f, 1.2f);

        const float norm = juce::jlimit (0.0f, 2.0f, autoGain);
        auto fill = meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, norm * 0.5f));
        g.setColour (accent);
        g.fillRoundedRectangle (fill, 6.0f);

        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Auto gain factor: " + juce::String (autoGain, 2),
                          meter.toNearestInt().translated (0, 22),
                          juce::Justification::centredLeft, 1);
    }

    void timerCallback() override
    {
        hpfFreq  = state.getRawParameterValue (kParamHpf)->load();
        lpfFreq  = state.getRawParameterValue (kParamLpf)->load();
        pan      = juce::jlimit (-1.0f, 1.0f, state.getRawParameterValue (kParamPan)->load());
        slope    = (int) state.getRawParameterValue (kParamSlope)->load();
        autoGain = processor.getAutoGainMeter();
        repaint();
    }
};
} // namespace

const std::array<GLSChannelPilotAudioProcessor::Preset, 3> GLSChannelPilotAudioProcessor::presetBank {{
    { "Drum Clean", {
        { kParamInputTrim,   0.0f },
        { kParamHpf,        80.0f },
        { kParamLpf,     16000.0f },
        { kParamPhase,       0.0f },
        { kParamPan,         0.0f },
        { kParamOutput,      0.0f },
        { kParamSlope,       1.0f }, // 24 dB
        { kParamAutoGain,    1.0f },
        { kParamBypass,      0.0f }
    }},
    { "Vox Prep", {
        { kParamInputTrim,  -1.0f },
        { kParamHpf,       120.0f },
        { kParamLpf,     14000.0f },
        { kParamPhase,      0.0f },
        { kParamPan,        0.0f },
        { kParamOutput,     0.0f },
        { kParamSlope,      0.0f }, // 12 dB
        { kParamAutoGain,   1.0f },
        { kParamBypass,     0.0f }
    }},
    { "Guitar Wide", {
        { kParamInputTrim,   0.0f },
        { kParamHpf,        90.0f },
        { kParamLpf,     12000.0f },
        { kParamPhase,       0.0f },
        { kParamPan,         0.15f },
        { kParamOutput,     -0.5f },
        { kParamSlope,       0.0f },
        { kParamAutoGain,    0.0f },
        { kParamBypass,      0.0f }
    }}
}};

GLSChannelPilotAudioProcessor::GLSChannelPilotAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GLSChannelPilotAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureFilterStateSize();
}

void GLSChannelPilotAudioProcessor::releaseResources()
{
}

void GLSChannelPilotAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    const auto inputTrimDb   = apvts.getRawParameterValue (kParamInputTrim)->load();
    const auto hpfFreq       = apvts.getRawParameterValue (kParamHpf)->load();
    const auto lpfFreq       = apvts.getRawParameterValue (kParamLpf)->load();
    const auto phaseInvert   = apvts.getRawParameterValue (kParamPhase)->load() > 0.5f;
    const auto panValue      = juce::jlimit (-1.0f, 1.0f, apvts.getRawParameterValue (kParamPan)->load());
    const auto outputTrimDb  = apvts.getRawParameterValue (kParamOutput)->load();
    const auto slopeChoice   = (int) apvts.getRawParameterValue (kParamSlope)->load();
    const bool autoGainEnabled = apvts.getRawParameterValue (kParamAutoGain)->load() > 0.5f;

    ensureFilterStateSize();
    updateFilterCoefficients (hpfFreq, lpfFreq, slopeChoice);

    const auto inputGain  = juce::Decibels::decibelsToGain (inputTrimDb);
    const auto outputGain = juce::Decibels::decibelsToGain (outputTrimDb);
    const bool useSecondStage = slopeChoice > 0;
    double inputEnergy = 0.0;
    double filteredEnergy = 0.0;

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        auto index = juce::jlimit (0, (int) filterPairs.size() - 1, ch);
        auto& hpfStages = filterPairs[(size_t) index].highPass;
        auto& lpfStages = filterPairs[(size_t) index].lowPass;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float value = channelData[sample] * inputGain;
            inputEnergy += value * value;

            value = hpfStages[0].processSample (value);
            if (useSecondStage)
                value = hpfStages[1].processSample (value);

            value = lpfStages[0].processSample (value);
            if (useSecondStage)
                value = lpfStages[1].processSample (value);

            if (phaseInvert)
                value = -value;

            channelData[sample] = value;
            filteredEnergy += value * value;
        }
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1)
    {
        for (int ch = 1; ch < totalNumOutputChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    float autoGainFactor = 1.0f;
    if (autoGainEnabled && inputEnergy > 1.0e-6 && filteredEnergy > 1.0e-6)
    {
        const auto ratio = std::sqrt ((inputEnergy + 1.0e-9) / (filteredEnergy + 1.0e-9));
        const auto limited = juce::jlimit (0.25f, 4.0f, (float) ratio);
        autoGainState = 0.85f * autoGainState + 0.15f * limited;
        autoGainFactor = autoGainState;
    }
    else if (autoGainEnabled)
    {
        autoGainState = 0.9f * autoGainState + 0.1f;
        autoGainFactor = autoGainState;
    }
    else
    {
        autoGainState = 1.0f;
        autoGainFactor = 1.0f;
    }

    lastAutoGain.store (autoGainFactor);

    const float appliedOutputGain = outputGain * autoGainFactor;
    const float panAngle = (panValue + 1.0f) * (juce::MathConstants<float>::pi * 0.25f);
    const float panLeft  = std::cos (panAngle);
    const float panRight = std::sin (panAngle);

    if (totalNumOutputChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = left[i] * panLeft * appliedOutputGain;
            const float r = right[i] * panRight * appliedOutputGain;

            left[i]  = l;
            right[i] = r;
        }
    }
    else
    {
        buffer.applyGain (appliedOutputGain);
    }
}

void GLSChannelPilotAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void GLSChannelPilotAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout GLSChannelPilotAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHpf, "HPF Freq",
                                                                   juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.3f), 60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLpf, "LPF Freq",
                                                                   juce::NormalisableRange<float> (4000.0f, 20000.0f, 0.01f, 0.4f), 12000.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamPhase, "Phase", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamPan, "Pan",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (kParamSlope, "Filter Slope",
                                                                    juce::StringArray { "12 dB", "24 dB" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamAutoGain, "Auto Gain", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GLSChannelPilotAudioProcessor::updateFilterCoefficients (float hpfFreq, float lpfFreq, int slopeChoice)
{
    const bool useSecondStage = slopeChoice > 0;
    const auto setup = [&](juce::dsp::IIR::Filter<float>& filter, float freq, bool isHigh)
    {
        if (isHigh)
            filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, freq, 0.707f);
        else
            filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, freq, 0.707f);
    };

    for (auto& pair : filterPairs)
    {
        setup (pair.highPass[0], hpfFreq, true);
        setup (pair.lowPass[0],  lpfFreq, false);
        if (useSecondStage)
        {
            setup (pair.highPass[1], hpfFreq, true);
            setup (pair.lowPass[1],  lpfFreq, false);
        }
        else
        {
            pair.highPass[1].reset();
            pair.lowPass[1].reset();
        }
    }
}

void GLSChannelPilotAudioProcessor::ensureFilterStateSize()
{
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    if ((int) filterPairs.size() < channels)
    {
        filterPairs.resize ((size_t) channels);
        for (auto& pair : filterPairs)
        {
            for (auto* f : { &pair.highPass[0], &pair.highPass[1], &pair.lowPass[0], &pair.lowPass[1] })
                f->reset();
        }
    }
}

//==============================================================================
GLSChannelPilotAudioProcessorEditor::GLSChannelPilotAudioProcessorEditor (GLSChannelPilotAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.ChannelPilot", "Channel Pilot")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    heroComponent = std::make_unique<ChannelPilotHeroComponent> (processorRef, processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*heroComponent);

    configureSlider (hpfSlider, "HPF");
    configureSlider (lpfSlider, "LPF");
    configureSlider (panSlider, "Pan");
    configureSlider (inputTrimSlider, "Input");
    configureSlider (outputTrimSlider, "Output");

    configureToggle (phaseButton, "Phase");
    configureToggle (autoGainButton, "Auto Gain");
    configureToggle (bypassButton, "Soft Bypass");
    configureComboBox (filterSlopeBox);

    auto& state = processorRef.getValueTreeState();
    hpfAttachment       = std::make_unique<SliderAttachment> (state, kParamHpf,       hpfSlider);
    lpfAttachment       = std::make_unique<SliderAttachment> (state, kParamLpf,       lpfSlider);
    panAttachment       = std::make_unique<SliderAttachment> (state, kParamPan,       panSlider);
    inputTrimAttachment = std::make_unique<SliderAttachment> (state, kParamInputTrim, inputTrimSlider);
    outputTrimAttachment= std::make_unique<SliderAttachment> (state, kParamOutput,    outputTrimSlider);
    slopeAttachment     = std::make_unique<ComboBoxAttachment> (state, kParamSlope,   filterSlopeBox);
    phaseAttachment     = std::make_unique<ButtonAttachment> (state, kParamPhase,     phaseButton);
    autoGainAttachment  = std::make_unique<ButtonAttachment> (state, kParamAutoGain,  autoGainButton);
    bypassAttachment    = std::make_unique<ButtonAttachment> (state, kParamBypass,    bypassButton);

    setSize (920, 460);
}

GLSChannelPilotAudioProcessorEditor::~GLSChannelPilotAudioProcessorEditor()
{
    for (auto* slider : { &hpfSlider, &lpfSlider, &panSlider, &inputTrimSlider, &outputTrimSlider })
        slider->setLookAndFeel (nullptr);

    for (auto* toggle : { &phaseButton, &autoGainButton, &bypassButton })
        toggle->setLookAndFeel (nullptr);

    filterSlopeBox.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSChannelPilotAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSChannelPilotAudioProcessorEditor::configureToggle (juce::ToggleButton& button, const juce::String& text)
{
    button.setLookAndFeel (&lookAndFeel);
    button.setButtonText (text);
    addAndMakeVisible (button);
}

void GLSChannelPilotAudioProcessorEditor::configureComboBox (juce::ComboBox& box)
{
    box.setLookAndFeel (&lookAndFeel);
    box.addItemList ({ "12 dB", "24 dB" }, 1);
    addAndMakeVisible (box);
}

void GLSChannelPilotAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.35f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GLSChannelPilotAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto body = bounds.reduced (12);
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.35f)).reduced (8);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.30f)).reduced (8);
    auto centre = body.reduced (8);

    hpfSlider.setBounds (left.removeFromTop (left.getHeight() / 3).reduced (6));
    lpfSlider.setBounds (left.removeFromTop (left.getHeight() / 2).reduced (6));
    filterSlopeBox.setBounds (left.removeFromTop (32));
    phaseButton.setBounds (left.removeFromTop (32));

    panSlider.setBounds (right.removeFromTop (right.getHeight() * 0.6f).reduced (6));
    autoGainButton.setBounds (right.removeFromTop (32).reduced (4));

    if (heroComponent != nullptr)
        heroComponent->setBounds (centre);

    auto footerArea = footerComponent.getBounds().reduced (24, 10);
    auto width = footerArea.getWidth() / 3;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (width).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (width).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (width).reduced (8, 14));
}

juce::AudioProcessorEditor* GLSChannelPilotAudioProcessor::createEditor()
{
    return new GLSChannelPilotAudioProcessorEditor (*this);
}

int GLSChannelPilotAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GLSChannelPilotAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GLSChannelPilotAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSChannelPilotAudioProcessor();
}

void GLSChannelPilotAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return;

    const auto& preset = presetBank[(size_t) index];
    for (const auto& entry : preset.params)
    {
        if (auto* param = apvts.getParameter (entry.first))
        {
            auto norm = param->getNormalisableRange().convertTo0to1 (entry.second);
            param->setValueNotifyingHost (norm);
        }
    }
}
