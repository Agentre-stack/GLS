#include "EQDynBandAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "DYN_BAND";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<EQDynBandAudioProcessor::Preset, 3> EQDynBandAudioProcessor::presetBank {{
    { "De-Ess Air", {
        { "band1_freq",    6500.0f },
        { "band1_q",          3.0f },
        { "band1_thresh",   -28.0f },
        { "band1_range",     -6.0f },
        { "band2_freq",    9500.0f },
        { "band2_q",          2.0f },
        { "band2_thresh",   -30.0f },
        { "band2_range",     -4.0f },
        { "mix",              0.85f },
        { kParamInput,        0.0f },
        { kParamOutput,      -0.5f },
        { kParamBypass,       0.0f }
    }},
    { "Mud Tamer", {
        { "band1_freq",     220.0f },
        { "band1_q",          1.4f },
        { "band1_thresh",   -26.0f },
        { "band1_range",     -5.0f },
        { "band2_freq",     550.0f },
        { "band2_q",          1.2f },
        { "band2_thresh",   -24.0f },
        { "band2_range",     -3.5f },
        { "mix",              0.9f },
        { kParamInput,        0.0f },
        { kParamOutput,       0.0f },
        { kParamBypass,       0.0f }
    }},
    { "Dynamic Sparkle", {
        { "band1_freq",    3500.0f },
        { "band1_q",          0.9f },
        { "band1_thresh",   -22.0f },
        { "band1_range",      3.0f },
        { "band2_freq",     9500.0f },
        { "band2_q",          1.1f },
        { "band2_thresh",   -20.0f },
        { "band2_range",      4.0f },
        { "mix",              0.8f },
        { kParamInput,       -0.5f },
        { kParamOutput,       0.5f },
        { kParamBypass,       0.0f }
    }}
}};

EQDynBandAudioProcessor::EQDynBandAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void EQDynBandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& band : band1States)
    {
        band.filter.prepare (spec);
        band.filter.reset();
        band.envelope = 0.0f;
        band.gain = 1.0f;
    }
    for (auto& band : band2States)
    {
        band.filter.prepare (spec);
        band.filter.reset();
        band.envelope = 0.0f;
        band.gain = 1.0f;
    }
}

void EQDynBandAudioProcessor::releaseResources()
{
}

void EQDynBandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto b1Freq    = get ("band1_freq");
    const auto b1Q       = get ("band1_q");
    const auto b1Thresh  = get ("band1_thresh");
    const auto b1Range   = get ("band1_range");
    const auto b2Freq    = get ("band2_freq");
    const auto b2Q       = get ("band2_q");
    const auto b2Thresh  = get ("band2_thresh");
    const auto b2Range   = get ("band2_range");
    const auto mix       = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float inputGain  = juce::Decibels::decibelsToGain (get (kParamInput));
    const float outputGain = juce::Decibels::decibelsToGain (get (kParamOutput));
    const bool bypassed    = get (kParamBypass) > 0.5f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        updateBandFilters (band1States[ch], b1Freq, b1Q);
        updateBandFilters (band2States[ch], b2Freq, b2Q);
    }

    const float attackMs = 10.0f;
    const float releaseMs = 120.0f;
    const float attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * (float) currentSampleRate));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& band1 = band1States[ch];
        auto& band2 = band2States[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float input = data[i];

            auto processBand = [&](DynamicBand& band, float thresh, float range)
            {
                const float bandSample = band.filter.processSample (input);
                const float level = std::abs (bandSample) + 1.0e-6f;
                if (level > band.envelope)
                    band.envelope = attackCoeff * band.envelope + (1.0f - attackCoeff) * level;
                else
                    band.envelope = releaseCoeff * band.envelope + (1.0f - releaseCoeff) * level;

                const float envDb = juce::Decibels::gainToDecibels (band.envelope);
                const float gainDb = computeGainDb (envDb, thresh, range);
                const float targetGain = juce::Decibels::decibelsToGain (gainDb);
                band.gain += 0.02f * (targetGain - band.gain);

                return std::pair<float, float> { bandSample, bandSample * band.gain };
            };

            const auto [band1Original, band1Processed] = processBand (band1, b1Thresh, b1Range);
            const auto [band2Original, band2Processed] = processBand (band2, b2Thresh, b2Range);

            data[i] = input + (band1Processed - band1Original) + (band2Processed - band2Original);
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

void EQDynBandAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQDynBandAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQDynBandAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto freqRange   = juce::NormalisableRange<float> (40.0f, 12000.0f, 0.01f, 0.4f);
    auto qRange      = juce::NormalisableRange<float> (0.2f, 10.0f, 0.001f, 0.5f);
    auto threshRange = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto rangeRange  = juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_freq",   "Band1 Freq",   freqRange,   250.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_q",      "Band1 Q",      qRange,      1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_thresh", "Band1 Thresh", threshRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_range",  "Band1 Range",  rangeRange,  -6.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_freq",   "Band2 Freq",   freqRange,   4000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_q",      "Band2 Q",      qRange,      1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_thresh", "Band2 Thresh", threshRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_range",  "Band2 Range",  rangeRange,  -6.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,    "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput,   "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass,   "Soft Bypass", false));

    return { params.begin(), params.end() };
}

EQDynBandAudioProcessorEditor::EQDynBandAudioProcessorEditor (EQDynBandAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("EQ")),
      headerComponent ("EQ.DynBand", "Dyn Band")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false)
    {
        initSlider (slider, label, macro);
    };

    make (band1FreqSlider,   "Band1 Freq", true);
    make (band1QSlider,      "Band1 Q", true);
    make (band1ThreshSlider, "Band1 Thresh");
    make (band1RangeSlider,  "Band1 Range");
    make (band2FreqSlider,   "Band2 Freq", true);
    make (band2QSlider,      "Band2 Q", true);
    make (band2ThreshSlider, "Band2 Thresh");
    make (band2RangeSlider,  "Band2 Range");
    make (mixSlider,         "Mix");
    make (inputTrimSlider,   "Input");
    make (outputTrimSlider,  "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "band1_freq", "band1_q", "band1_thresh", "band1_range",
        "band2_freq", "band2_q", "band2_thresh", "band2_range",
        "mix", kParamInput, kParamOutput
    };

    juce::Slider* sliders[] = {
        &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
        &band2FreqSlider, &band2QSlider, &band2ThreshSlider, &band2RangeSlider,
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (820, 460);
}

void EQDynBandAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto lab = std::make_unique<juce::Label>();
    lab->setText (name, juce::dontSendNotification);
    lab->setJustificationType (juce::Justification::centred);
    lab->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    lab->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*lab);
    labels.push_back (std::move (lab));
}

void EQDynBandAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void EQDynBandAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
        &band2FreqSlider, &band2QSlider, &band2ThreshSlider, &band2RangeSlider,
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

void EQDynBandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void EQDynBandAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.45f));
    auto mid = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.45f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 4;
    band1FreqSlider   .setBounds (top.removeFromLeft (topWidth).reduced (8));
    band1QSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));
    band1ThreshSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    band1RangeSlider  .setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto midWidth = mid.getWidth() / 4;
    band2FreqSlider   .setBounds (mid.removeFromLeft (midWidth).reduced (8));
    band2QSlider      .setBounds (mid.removeFromLeft (midWidth).reduced (8));
    band2ThreshSlider .setBounds (mid.removeFromLeft (midWidth).reduced (8));
    band2RangeSlider  .setBounds (mid.removeFromLeft (midWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 3;
    mixSlider        .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* EQDynBandAudioProcessor::createEditor()
{
    return new EQDynBandAudioProcessorEditor (*this);
}

void EQDynBandAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
    {
        band1States.clear();
        band2States.clear();
        dryBuffer.setSize (0, 0);
        return;
    }

    auto prepareBand = [this](std::vector<DynamicBand>& bands, int required)
    {
        if ((int) bands.size() < required)
        {
            const int previous = (int) bands.size();
            bands.resize ((size_t) required);

            juce::dsp::ProcessSpec spec {
                currentSampleRate > 0.0 ? currentSampleRate : 44100.0,
                lastBlockSize > 0 ? lastBlockSize : 512u,
                1
            };

            for (int ch = previous; ch < required; ++ch)
            {
                bands[(size_t) ch].filter.prepare (spec);
                bands[(size_t) ch].filter.reset();
                bands[(size_t) ch].envelope = 0.0f;
                bands[(size_t) ch].gain = 1.0f;
            }
        }
    };

    prepareBand (band1States, numChannels);
    prepareBand (band2States, numChannels);
    dryBuffer.setSize (numChannels, (int) (lastBlockSize > 0 ? lastBlockSize : 512u), false, false, true);
}

void EQDynBandAudioProcessor::updateBandFilters (DynamicBand& bandState, float freq, float q)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clampedFreq = juce::jlimit (40.0f, (float) (currentSampleRate * 0.49f), freq);
    const auto clampedQ    = juce::jlimit (0.2f, 10.0f, q);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, clampedFreq, clampedQ);
    bandState.filter.coefficients = coeffs;
}

float EQDynBandAudioProcessor::computeGainDb (float envDb, float threshDb, float rangeDb) const
{
    if (rangeDb >= 0.0f)
    {
        if (envDb >= threshDb)
            return 0.0f;

        const float deficit = juce::jlimit (0.0f, 24.0f, threshDb - envDb);
        return (deficit / 24.0f) * rangeDb;
    }

    if (envDb <= threshDb)
        return 0.0f;

    const float excess = juce::jlimit (0.0f, 24.0f, envDb - threshDb);
    return -(excess / 24.0f) * std::abs (rangeDb);
}

int EQDynBandAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String EQDynBandAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void EQDynBandAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void EQDynBandAudioProcessor::applyPreset (int index)
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
    return new EQDynBandAudioProcessor();
}
