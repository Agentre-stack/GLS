#include "DYNVocalPinAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "VOCAL_PIN";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<DYNVocalPinAudioProcessor::Preset, 3> DYNVocalPinAudioProcessor::presetBank {{
    { "Vocal Level", {
        { "thresh",     -18.0f },
        { "ratio",        3.5f },
        { "attack",       6.0f },
        { "release",    120.0f },
        { "deess_freq", 6500.0f },
        { "deess_amount", 0.4f },
        { "mix",          0.9f },
        { kParamInput,    0.0f },
        { kParamOutput,   0.5f },
        { kParamBypass,   0.0f }
    }},
    { "Air Tame", {
        { "thresh",     -16.0f },
        { "ratio",        2.8f },
        { "attack",       8.0f },
        { "release",    200.0f },
        { "deess_freq", 7200.0f },
        { "deess_amount", 0.6f },
        { "mix",          0.85f },
        { kParamInput,    0.0f },
        { kParamOutput,   0.0f },
        { kParamBypass,   0.0f }
    }},
    { "Broadcast Pin", {
        { "thresh",     -14.0f },
        { "ratio",        2.2f },
        { "attack",       4.0f },
        { "release",    140.0f },
        { "deess_freq", 5800.0f },
        { "deess_amount", 0.5f },
        { "mix",          0.78f },
        { kParamInput,   -1.0f },
        { kParamOutput,   0.0f },
        { kParamBypass,   0.0f }
    }}
}};

DYNVocalPinAudioProcessor::DYNVocalPinAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNVocalPinAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
    updateDeEssFilters (6000.0f);
}

void DYNVocalPinAudioProcessor::releaseResources()
{
}

void DYNVocalPinAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb    = get ("thresh");
    const auto ratio       = juce::jmax (1.0f, get ("ratio"));
    const auto attackMs    = juce::jmax (0.1f, get ("attack"));
    const auto releaseMs   = juce::jmax (0.1f, get ("release"));
    const auto deEssFreq   = get ("deess_freq");
    const auto deEssAmount = juce::jlimit (0.0f, 1.0f, get ("deess_amount"));
    const auto mix         = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const auto inputGain   = juce::Decibels::decibelsToGain (get (kParamInput));
    const auto outputGain  = juce::Decibels::decibelsToGain (get (kParamOutput));
    const bool bypassed    = get (kParamBypass) > 0.5f;

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    dryBuffer.makeCopyOf (buffer, true);
    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels());
    updateDeEssFilters (deEssFreq);

    for (auto& follower : compFollowers)
    {
        follower.setSampleRate (currentSampleRate);
        follower.setTimes (attackMs, releaseMs);
    }

    for (auto& follower : deEssFollowers)
    {
        follower.setSampleRate (currentSampleRate);
        follower.setTimes (juce::jmax (0.1f, attackMs * 0.25f),
                           juce::jmax (1.0f, releaseMs * 0.5f));
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* filter = &deEssFilters[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = data[i];
            float sample = drySample;

            const float env = compFollowers[ch].process (sample) + 1.0e-6f;
            const float envDb = juce::Decibels::gainToDecibels (env);
            sample *= computeGain (envDb, threshDb, ratio);

            const float sibilant = filter->processSample (sample);
            const float essLevel = deEssFollowers[ch].process (sibilant);
            const float essNorm = juce::jlimit (0.0f, 1.0f, essLevel * 8.0f);
            const float essAttenuation = deEssAmount * essNorm;
            sample -= sibilant * essAttenuation;

            data[i] = sample;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = (wet[i] * mix + dry[i] * (1.0f - mix)) * outputGain;
    }
}

void DYNVocalPinAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void DYNVocalPinAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData (data, sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNVocalPinAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",     "Threshold",
                                                                   juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",      "Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 12.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",     "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",    "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 600.0f, 0.01f, 0.35f), 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deess_freq", "DeEss Freq",
                                                                   juce::NormalisableRange<float> (2000.0f, 12000.0f, 0.01f, 0.35f), 6000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deess_amount","DeEss Amount",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,  "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNVocalPinAudioProcessorEditor::DYNVocalPinAudioProcessorEditor (DYNVocalPinAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.VocalPin", "Vocal Pin")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& s, const juce::String& l, bool macro = false) { initSlider (s, l, macro); };

    make (threshSlider,     "Thresh", true);
    make (ratioSlider,      "Ratio", true);
    make (attackSlider,     "Attack");
    make (releaseSlider,    "Release");
    make (deEssFreqSlider,  "DeEss Freq");
    make (deEssAmountSlider,"DeEss Amt");
    make (mixSlider,        "Mix");
    make (inputTrimSlider,  "Input");
    make (outputTrimSlider, "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "thresh", "ratio", "attack", "release", "deess_freq", "deess_amount", "mix", kParamInput, kParamOutput };
    juce::Slider* sliders[]      = { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                     &deEssFreqSlider, &deEssAmountSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (820, 420);
}

void DYNVocalPinAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto label = std::make_unique<juce::Label>();
    label->setText (name, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    labels.push_back (std::move (label));
}

void DYNVocalPinAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNVocalPinAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
        &deEssFreqSlider, &deEssAmountSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void DYNVocalPinAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNVocalPinAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto topRow = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottomRow = area;

    auto topWidth = topRow.getWidth() / 4;
    threshSlider .setBounds (topRow.removeFromLeft (topWidth).reduced (8));
    ratioSlider  .setBounds (topRow.removeFromLeft (topWidth).reduced (8));
    attackSlider .setBounds (topRow.removeFromLeft (topWidth).reduced (8));
    releaseSlider.setBounds (topRow.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottomRow.getWidth() / 5;
    deEssFreqSlider  .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    deEssAmountSlider.setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    mixSlider        .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider  .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* DYNVocalPinAudioProcessor::createEditor()
{
    return new DYNVocalPinAudioProcessorEditor (*this);
}

void DYNVocalPinAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) compFollowers.size() < numChannels)
    {
        compFollowers.resize (numChannels);
        deEssFollowers.resize (numChannels);
    }

    if ((int) deEssFilters.size() < numChannels)
    {
        const int previousSize = (int) deEssFilters.size();
        deEssFilters.resize (numChannels);

        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        for (int i = previousSize; i < numChannels; ++i)
        {
            deEssFilters[i].prepare (spec);
            deEssFilters[i].reset();
        }
    }
    else
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        for (auto& filter : deEssFilters)
            filter.prepare (spec);
    }
}

void DYNVocalPinAudioProcessor::updateDeEssFilters (float freq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto limitedFreq = juce::jlimit (800.0f, (float) (currentSampleRate * 0.45), freq);
    const auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, limitedFreq, 2.0f);

    for (auto& filter : deEssFilters)
        filter.coefficients = coeffs;
}

float DYNVocalPinAudioProcessor::computeGain (float levelDb, float threshDb, float ratio) const
{
    if (ratio <= 1.0f || levelDb <= threshDb)
        return 1.0f;

    const float over = levelDb - threshDb;
    const float compressed = threshDb + over / ratio;
    return juce::Decibels::decibelsToGain (compressed - levelDb);
}

int DYNVocalPinAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNVocalPinAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNVocalPinAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNVocalPinAudioProcessor::applyPreset (int index)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DYNVocalPinAudioProcessor();
}
