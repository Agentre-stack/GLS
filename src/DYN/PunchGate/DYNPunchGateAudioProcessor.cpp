#include "DYNPunchGateAudioProcessor.h"

namespace
{
constexpr auto kParamThresh      = "thresh";
constexpr auto kParamRange       = "range";
constexpr auto kParamAttack      = "attack";
constexpr auto kParamHold        = "hold";
constexpr auto kParamRelease     = "release";
constexpr auto kParamHysteresis  = "hysteresis";
constexpr auto kParamPunchBoost  = "punch_boost";
constexpr auto kParamSidechainHPF= "sc_hpf";
constexpr auto kParamSidechainLPF= "sc_lpf";
constexpr auto kParamMix         = "mix";
constexpr auto kParamInputTrim   = "input_trim";
constexpr auto kParamOutputTrim  = "output_trim";
constexpr auto kParamBypass      = "ui_bypass";
}

const std::array<DYNPunchGateAudioProcessor::Preset, 3> DYNPunchGateAudioProcessor::presetBank {{
    { "Drum Gate", {
        { kParamThresh,     -25.0f },
        { kParamRange,       32.0f },
        { kParamAttack,       1.5f },
        { kParamHold,        40.0f },
        { kParamRelease,    120.0f },
        { kParamHysteresis,   6.0f },
        { kParamPunchBoost,   6.0f },
        { kParamSidechainHPF, 70.0f },
        { kParamSidechainLPF,8000.0f },
        { kParamMix,          1.0f },
        { kParamInputTrim,    0.0f },
        { kParamOutputTrim,   0.0f },
        { kParamBypass,       0.0f }
    }},
    { "Vox Tight", {
        { kParamThresh,     -32.0f },
        { kParamRange,       24.0f },
        { kParamAttack,       2.0f },
        { kParamHold,        25.0f },
        { kParamRelease,    160.0f },
        { kParamHysteresis,   4.0f },
        { kParamPunchBoost,   3.0f },
        { kParamSidechainHPF,120.0f },
        { kParamSidechainLPF,9000.0f },
        { kParamMix,          0.9f },
        { kParamInputTrim,    0.0f },
        { kParamOutputTrim,   0.0f },
        { kParamBypass,       0.0f }
    }},
    { "Guitar Chug", {
        { kParamThresh,     -28.0f },
        { kParamRange,       36.0f },
        { kParamAttack,       1.0f },
        { kParamHold,        60.0f },
        { kParamRelease,    200.0f },
        { kParamHysteresis,   5.0f },
        { kParamPunchBoost,   4.0f },
        { kParamSidechainHPF, 90.0f },
        { kParamSidechainLPF,7000.0f },
        { kParamMix,          0.95f },
        { kParamInputTrim,    0.0f },
        { kParamOutputTrim,   0.0f },
        { kParamBypass,       0.0f }
    }}
}};

DYNPunchGateAudioProcessor::DYNPunchGateAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PUNCH_GATE", createParameterLayout())
{
}

void DYNPunchGateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  (juce::uint32) juce::jmax (1, samplesPerBlock),
                                  1 };
    for (auto& filter : scHighPassFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : scLowPassFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& state : channelStates)
    {
        state.envelope = 0.0f;
        state.holdCounter = 0.0f;
        state.gateGain = 1.0f;
    }
}

void DYNPunchGateAudioProcessor::releaseResources()
{
}

void DYNPunchGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb    = read (kParamThresh);
    const auto rangeDb     = juce::jmax (0.0f, read (kParamRange));
    const auto attackMs    = juce::jmax (0.1f, read (kParamAttack));
    const auto holdMs      = juce::jmax (0.0f, read (kParamHold));
    const auto releaseMs   = juce::jmax (1.0f, read (kParamRelease));
    const auto hysteresis  = juce::jmax (0.0f, read (kParamHysteresis));
    const auto punchBoost  = juce::Decibels::decibelsToGain (read (kParamPunchBoost));
    const auto scHpf       = read (kParamSidechainHPF);
    const auto scLpf       = read (kParamSidechainLPF);
    const auto mix         = juce::jlimit (0.0f, 1.0f, read (kParamMix));
    const auto inputTrim   = juce::Decibels::decibelsToGain (read (kParamInputTrim));
    const auto outputTrim  = juce::Decibels::decibelsToGain (read (kParamOutputTrim));

    ensureStateSize();

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto openThresh   = juce::Decibels::decibelsToGain (threshDb);
    const auto closeThresh  = juce::Decibels::decibelsToGain (threshDb + hysteresis);
    const auto attenuation  = juce::Decibels::decibelsToGain (-rangeDb);
    const auto holdSamples  = holdMs * 0.001f * currentSampleRate;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);

    juce::AudioBuffer<float> sidechainBuffer;
    const bool hasSidechainBus = getBusCount (true) > 1;
    if (hasSidechainBus)
        sidechainBuffer = getBusBuffer (buffer, true, 1);
    const bool sidechainAvailable = hasSidechainBus
                                    && sidechainBuffer.getNumChannels() > 0
                                    && sidechainBuffer.getNumSamples() == numSamples;

    auto makeFilter = [this, scHpf, scLpf]()
    {
        auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                     juce::jlimit (20.0f, 2000.0f, scHpf));
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate,
                                                                     juce::jlimit (200.0f, (float) (currentSampleRate * 0.45f), scLpf));
        for (auto& filter : scHighPassFilters)
            filter.coefficients = hp;
        for (auto& filter : scLowPassFilters)
            filter.coefficients = lp;
    };
    makeFilter();

    float meterValue = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);
        const float* scData = nullptr;
        if (sidechainAvailable)
        {
            const int scChannels = sidechainBuffer.getNumChannels();
            const int scIndex = juce::jlimit (0, scChannels - 1, ch);
            scData = sidechainBuffer.getReadPointer (scIndex);
        }
        auto& hpf = scHighPassFilters[juce::jmin (ch, (int) scHighPassFilters.size() - 1)];
        auto& lpf = scLowPassFilters[juce::jmin (ch, (int) scLowPassFilters.size() - 1)];

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            float detectorSample = scData != nullptr ? scData[i] : sample;
            detectorSample = hpf.processSample (detectorSample);
            detectorSample = lpf.processSample (detectorSample);
            const float level  = std::abs (detectorSample);

            if (level > state.envelope)
                state.envelope = attackCoeff * state.envelope + (1.0f - attackCoeff) * level;
            else
                state.envelope = releaseCoeff * state.envelope + (1.0f - releaseCoeff) * level;

            if (state.envelope >= openThresh)
            {
                state.holdCounter = holdSamples;
                state.gateGain = punchBoost;
            }
            else if (state.holdCounter > 0.0f)
            {
                state.holdCounter -= 1.0f;
            }
            else if (state.envelope <= closeThresh)
            {
                state.gateGain += 0.01f * (attenuation - state.gateGain);
            }

            data[i] = sample * state.gateGain;

            if (state.gateGain > 1.0f)
                state.gateGain += 0.003f * (1.0f - state.gateGain);
            meterValue = juce::jmax (meterValue, state.gateGain);
        }
    }

    if (mix < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* processed = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                processed[i] = processed[i] * mix + dry[i] * (1.0f - mix);
        }
    }

    buffer.applyGain (outputTrim);
    gateMeter.store (juce::jlimit (0.0f, 1.0f, meterValue));
}

int DYNPunchGateAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNPunchGateAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNPunchGateAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNPunchGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNPunchGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNPunchGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamThresh, "Threshold",
                                                                   juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamRange, "Range",
                                                                   juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamAttack, "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.35f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHold, "Hold",
                                                                   juce::NormalisableRange<float> (0.0f, 200.0f, 0.01f, 0.35f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamRelease, "Release",
                                                                   juce::NormalisableRange<float> (5.0f, 500.0f, 0.01f, 0.3f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHysteresis, "Hysteresis",
                                                                   juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamPunchBoost, "Punch Boost",
                                                                   juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamSidechainHPF, "SC HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.45f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamSidechainLPF, "SC LPF",
                                                                   juce::NormalisableRange<float> (500.0f, 20000.0f, 0.01f, 0.45f), 8000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

namespace
{
class GateVisualComponent : public juce::Component, private juce::Timer
{
public:
    GateVisualComponent (DYNPunchGateAudioProcessor& processorRef, juce::Colour accentColour)
        : processor (processorRef), accent (accentColour)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.6f);

        auto meterArea = bounds.removeFromRight (64.0f).reduced (10.0f);
        const float gateValue = juce::jlimit (0.0f, 1.0f, processor.getGateMeter());
        auto fill = meterArea.withHeight (meterArea.getHeight() * gateValue)
                             .withY (meterArea.getBottom() - meterArea.getHeight() * gateValue);
        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (fill, 6.0f);
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Gate", meterArea.toNearestInt().translated (0, -18), juce::Justification::centred, 1);

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        auto infoArea = bounds.reduced (16.0f);
        g.drawFittedText ("Transient-friendly gate with punch boost.\n"
                          "Use Sidechain filters to key from an external bus.",
                          infoArea.toNearestInt(), juce::Justification::centredLeft, 4);
    }

private:
    DYNPunchGateAudioProcessor& processor;
    juce::Colour accent;

    void timerCallback() override { repaint(); }
};
} // namespace

DYNPunchGateAudioProcessorEditor::DYNPunchGateAudioProcessorEditor (DYNPunchGateAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.PunchGate", "Punch Gate")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    gateVisual = std::make_unique<GateVisualComponent> (processorRef, accentColour);
    addAndMakeVisible (*gateVisual);

    auto makeKnob = [this](juce::Slider& slider, const juce::String& label)
    {
        initialiseSlider (slider, label);
    };

    makeKnob (threshSlider,    "Threshold");
    makeKnob (rangeSlider,     "Range");
    makeKnob (attackSlider,    "Attack");
    makeKnob (holdSlider,      "Hold");
    makeKnob (releaseSlider,   "Release");
    makeKnob (hysteresisSlider,"Hysteresis");
    makeKnob (punchBoostSlider,"Punch");
    initialiseSlider (sidechainHpfSlider, "SC HPF");
    initialiseSlider (sidechainLpfSlider, "SC LPF");
    initialiseSlider (mixSlider,          "Blend");
    initialiseSlider (inputTrimSlider,    "Input");
    initialiseSlider (outputTrimSlider,   "Output");
    configureToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamThresh,      threshSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamRange,       rangeSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamAttack,      attackSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamHold,        holdSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamRelease,     releaseSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamHysteresis,  hysteresisSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamPunchBoost,  punchBoostSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamSidechainHPF, sidechainHpfSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamSidechainLPF, sidechainLpfSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamMix,          mixSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamInputTrim,    inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, kParamOutputTrim,   outputTrimSlider));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (960, 520);
}

DYNPunchGateAudioProcessorEditor::~DYNPunchGateAudioProcessorEditor()
{
    for (auto& entry : labeledSliders)
        if (entry.slider != nullptr)
            entry.slider->setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DYNPunchGateAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (slider);

    LabeledSlider labeled { &slider, std::make_unique<juce::Label>() };
    labeled.label->setText (name, juce::dontSendNotification);
    labeled.label->setJustificationType (juce::Justification::centred);
    labeled.label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    labeled.label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*labeled.label);
    labeledSliders.push_back (std::move (labeled));
}

void DYNPunchGateAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNPunchGateAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        entry.label->setBounds (sliderBounds.withHeight (18).translated (0, -20));
    }
}

void DYNPunchGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void DYNPunchGateAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.33f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.27f)).reduced (12);
    auto centre = body.reduced (12);

    if (gateVisual != nullptr)
        gateVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 4;
    threshSlider   .setBounds (left.removeFromTop (macroHeight).reduced (8));
    rangeSlider    .setBounds (left.removeFromTop (macroHeight).reduced (8));
    attackSlider   .setBounds (left.removeFromTop (macroHeight).reduced (8));
    releaseSlider  .setBounds (left.removeFromTop (macroHeight).reduced (8));

    auto rightHeight = juce::jmax (1, right.getHeight() / 5);
    holdSlider       .setBounds (right.removeFromTop (rightHeight).reduced (8));
    hysteresisSlider .setBounds (right.removeFromTop (rightHeight).reduced (8));
    punchBoostSlider .setBounds (right.removeFromTop (rightHeight).reduced (8));
    sidechainHpfSlider.setBounds (right.removeFromTop (rightHeight).reduced (8));
    sidechainLpfSlider.setBounds (right.removeFromTop (rightHeight).reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 3;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    mixSlider       .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* DYNPunchGateAudioProcessor::createEditor()
{
    return new DYNPunchGateAudioProcessorEditor (*this);
}

void DYNPunchGateAudioProcessor::applyPreset (int index)
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

void DYNPunchGateAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = juce::jmax (1, getTotalNumOutputChannels());
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);

    auto ensureFilters = [this, requiredChannels](std::vector<juce::dsp::IIR::Filter<float>>& filters)
    {
        if ((int) filters.size() != requiredChannels)
            filters.resize (requiredChannels);

        juce::dsp::ProcessSpec spec {
            currentSampleRate > 0.0 ? currentSampleRate : 44100.0,
            (juce::uint32) juce::jmax (1, getBlockSize()),
            1
        };

        for (auto& filter : filters)
        {
            filter.prepare (spec);
            filter.reset();
        }
    };

    ensureFilters (scHighPassFilters);
    ensureFilters (scLowPassFilters);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DYNPunchGateAudioProcessor();
}
