#include "EQAirGlassAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "EQ_AIR_GLASS";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<EQAirGlassAudioProcessor::Preset, 3> EQAirGlassAudioProcessor::presetBank {{
    { "Pop Vocal Air", {
        { "air_freq",        12000.0f },
        { "air_gain",            5.5f },
        { "harmonic_blend",      0.35f },
        { "deharsh",             0.55f },
        { kParamInput,           0.0f },
        { kParamOutput,         -0.5f },
        { kParamBypass,          0.0f }
    }},
    { "Master Shimmer", {
        { "air_freq",        16000.0f },
        { "air_gain",            3.0f },
        { "harmonic_blend",      0.25f },
        { "deharsh",             0.35f },
        { kParamInput,           0.0f },
        { kParamOutput,          0.0f },
        { kParamBypass,          0.0f }
    }},
    { "Cymbal Brighten", {
        { "air_freq",        11000.0f },
        { "air_gain",            7.5f },
        { "harmonic_blend",      0.45f },
        { "deharsh",             0.65f },
        { kParamInput,           0.0f },
        { kParamOutput,         -1.0f },
        { kParamBypass,          0.0f }
    }}
}};

EQAirGlassAudioProcessor::EQAirGlassAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void EQAirGlassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& filter : airShelves)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : harshFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }

    std::fill (harshEnvelopes.begin(), harshEnvelopes.end(), 0.0f);
}

void EQAirGlassAudioProcessor::releaseResources()
{
}

void EQAirGlassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float airFreq       = get ("air_freq");
    const float airGainDb     = get ("air_gain");
    const float harmonicBlend = juce::jlimit (0.0f, 1.0f, get ("harmonic_blend"));
    const float deHarsh       = juce::jlimit (0.0f, 1.0f, get ("deharsh"));
    const float inputGain     = juce::Decibels::decibelsToGain (get (kParamInput));
    const float outputGain    = juce::Decibels::decibelsToGain (get (kParamOutput));
    const bool bypassed       = get (kParamBypass) > 0.5f;

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureStateSize (buffer.getNumChannels());

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    updateShelfCoefficients (airFreq, airGainDb);
    updateHarshFilters (airFreq * 0.8f);

    const float drive        = 1.0f + juce::jlimit (0.0f, 18.0f, airGainDb) / 12.0f;
    const float attackCoeff  = std::exp (-1.0f / (0.0025f * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (0.05f * (float) currentSampleRate));

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& shelf = airShelves[(size_t) ch];
        auto& harsh = harshFilters[(size_t) ch];
        float& env  = harshEnvelopes[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            float airy = shelf.processSample (sample);

            const float harmonic = std::tanh (airy * drive);
            airy = airy * (1.0f - harmonicBlend) + harmonic * harmonicBlend;

            const float harshBand = harsh.processSample (airy);
            const float level = std::abs (harshBand);
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const float reduction = deHarsh * juce::jlimit (0.0f, 1.0f, env * 8.0f);
            airy -= harshBand * reduction;

            data[i] = airy;
        }
    }

    buffer.applyGain (outputGain);
}

void EQAirGlassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void EQAirGlassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQAirGlassAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_freq",       "Air Freq",
                                                                   juce::NormalisableRange<float> (6000.0f, 20000.0f, 0.01f, 0.4f), 12000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_gain",       "Air Gain",
                                                                   juce::NormalisableRange<float> (-6.0f, 12.0f, 0.1f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("harmonic_blend", "Harmonic Blend",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deharsh",        "DeHarsh",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,      "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput,     "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass,     "Soft Bypass", false));

    return { params.begin(), params.end() };
}

EQAirGlassAudioProcessorEditor::EQAirGlassAudioProcessorEditor (EQAirGlassAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("EQ")),
      headerComponent ("EQ.AirGlass", "Air Glass")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& s, const juce::String& l, bool macro = false) { initSlider (s, l, macro); };
    make (airFreqSlider,       "Air Freq", true);
    make (airGainSlider,       "Air Gain", true);
    make (harmonicBlendSlider, "Blend");
    make (deHarshSlider,       "DeHarsh");
    make (inputTrimSlider,     "Input");
    make (outputTrimSlider,    "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "air_freq", "air_gain", "harmonic_blend", "deharsh", kParamInput, kParamOutput };
    juce::Slider* sliders[]     = { &airFreqSlider, &airGainSlider, &harmonicBlendSlider, &deHarshSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (760, 420);
}

void EQAirGlassAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (label);
    addAndMakeVisible (slider);

    auto lab = std::make_unique<juce::Label>();
    lab->setText (label, juce::dontSendNotification);
    lab->setJustificationType (juce::Justification::centred);
    lab->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    lab->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*lab);
    labels.push_back (std::move (lab));
}

void EQAirGlassAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void EQAirGlassAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &airFreqSlider, &airGainSlider, &harmonicBlendSlider, &deHarshSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void EQAirGlassAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void EQAirGlassAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    const int topWidth = top.getWidth() / 3;
    airFreqSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));
    airGainSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));
    harmonicBlendSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));

    const int bottomWidth = bottom.getWidth() / 3;
    deHarshSlider     .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider   .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* EQAirGlassAudioProcessor::createEditor()
{
    return new EQAirGlassAudioProcessorEditor (*this);
}

void EQAirGlassAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
    {
        airShelves.clear();
        harshFilters.clear();
        harshEnvelopes.clear();
        return;
    }

    const bool needsResize = (int) airShelves.size() < numChannels;
    if (needsResize)
    {
        juce::dsp::ProcessSpec spec {
            currentSampleRate > 0.0 ? currentSampleRate : 44100.0,
            lastBlockSize > 0 ? lastBlockSize : 512u,
            1
        };

        const int previous = (int) airShelves.size();
        airShelves.resize ((size_t) numChannels);
        harshFilters.resize ((size_t) numChannels);
        harshEnvelopes.resize ((size_t) numChannels, 0.0f);

        for (int ch = previous; ch < numChannels; ++ch)
        {
            airShelves[(size_t) ch].prepare (spec);
            airShelves[(size_t) ch].reset();
            harshFilters[(size_t) ch].prepare (spec);
            harshFilters[(size_t) ch].reset();
            harshEnvelopes[(size_t) ch] = 0.0f;
        }
    }
    else
    {
        harshEnvelopes.resize ((size_t) numChannels, 0.0f);
    }
}

void EQAirGlassAudioProcessor::updateShelfCoefficients (float freq, float gainDb)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clampedFreq = juce::jlimit (4000.0f, (float) (currentSampleRate * 0.49f), freq);
    const auto gainLinear  = juce::Decibels::decibelsToGain (gainDb);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, clampedFreq, 0.707f, gainLinear);

    for (auto& filter : airShelves)
        filter.coefficients = coeffs;
}

void EQAirGlassAudioProcessor::updateHarshFilters (float freq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clamped = juce::jlimit (2000.0f, (float) (currentSampleRate * 0.49f), freq);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, clamped, 1.2f);

    for (auto& filter : harshFilters)
        filter.coefficients = coeffs;
}

int EQAirGlassAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String EQAirGlassAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void EQAirGlassAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void EQAirGlassAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return;

    const auto& preset = presetBank[(size_t) index];
    for (const auto& entry : preset.params)
    {
        if (auto* param = apvts.getParameter (entry.first))
        {
            const auto norm = param->getNormalisableRange().convertTo0to1 (entry.second);
            param->setValueNotifyingHost (norm);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQAirGlassAudioProcessor();
}
