#include "DYNSmoothDestroyerAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "SMOOTH_DESTROYER";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<DYNSmoothDestroyerAudioProcessor::Preset, 3> DYNSmoothDestroyerAudioProcessor::presetBank {{
    { "Bus Tamer", {
        { "band1_freq",    220.0f },
        { "band1_q",         1.2f },
        { "band1_thresh",  -26.0f },
        { "band1_range",    -6.0f },
        { "band2_freq",   4200.0f },
        { "band2_q",         1.0f },
        { "band2_thresh",  -24.0f },
        { "band2_range",    -4.0f },
        { "global_attack",  15.0f },
        { "global_release",160.0f },
        { "mix",             0.8f },
        { kParamInput,       0.0f },
        { kParamOutput,      0.0f },
        { kParamBypass,      0.0f }
    }},
    { "Vocal De-Harsh", {
        { "band1_freq",    180.0f },
        { "band1_q",         1.4f },
        { "band1_thresh",  -30.0f },
        { "band1_range",    -4.0f },
        { "band2_freq",   6200.0f },
        { "band2_q",         2.0f },
        { "band2_thresh",  -32.0f },
        { "band2_range",    -8.0f },
        { "global_attack",  10.0f },
        { "global_release",140.0f },
        { "mix",             0.85f },
        { kParamInput,       0.0f },
        { kParamOutput,      0.5f },
        { kParamBypass,      0.0f }
    }},
    { "Guitar Smooth", {
        { "band1_freq",    160.0f },
        { "band1_q",         1.1f },
        { "band1_thresh",  -28.0f },
        { "band1_range",    -5.0f },
        { "band2_freq",   3200.0f },
        { "band2_q",         1.6f },
        { "band2_thresh",  -26.0f },
        { "band2_range",    -6.0f },
        { "global_attack",  12.0f },
        { "global_release",180.0f },
        { "mix",             0.8f },
        { kParamInput,      -0.5f },
        { kParamOutput,      0.0f },
        { kParamBypass,      0.0f }
    }}
}};

DYNSmoothDestroyerAudioProcessor::DYNSmoothDestroyerAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNSmoothDestroyerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize();
    for (auto* bandVector : { &band1States, &band2States })
        for (auto& band : *bandVector)
        {
            band.bandFilter.reset();
            band.envelope = 0.0f;
            band.gain = 1.0f;
        }
}

void DYNSmoothDestroyerAudioProcessor::releaseResources()
{
}

void DYNSmoothDestroyerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto b1Freq    = read ("band1_freq");
    const auto b1Q       = read ("band1_q");
    const auto b1Thresh  = read ("band1_thresh");
    const auto b1Range   = read ("band1_range");
    const auto b2Freq    = read ("band2_freq");
    const auto b2Q       = read ("band2_q");
    const auto b2Thresh  = read ("band2_thresh");
    const auto b2Range   = read ("band2_range");
    const auto attackMs  = read ("global_attack");
    const auto releaseMs = read ("global_release");
    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputGain = juce::Decibels::decibelsToGain (read (kParamInput));
    const auto outputGain= juce::Decibels::decibelsToGain (read (kParamOutput));
    const bool bypassed  = read (kParamBypass) > 0.5f;

    ensureStateSize();
    buffer.applyGain (inputGain);

    if (bypassed)
        return;

    dryBuffer.makeCopyOf (buffer, true);

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& band1 = band1States[ch];
        auto& band2 = band2States[ch];
        updateBandCoefficients (band1, b1Freq, b1Q);
        updateBandCoefficients (band2, b2Freq, b2Q);

        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            float band1Sample = band1.bandFilter.processSample (sample);
            float band2Sample = band2.bandFilter.processSample (sample);

            auto processBand = [&](DynamicBand& band, float input, float thresh, float range)
            {
                const float level = std::abs (input);
                auto& env = band.envelope;
                if (level > env)
                    env = attackCoeff * env + (1.0f - attackCoeff) * level;
                else
                    env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

                const auto envDb = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f));
                const auto gainDb = computeBandGain (envDb, thresh, range);
                const auto target = juce::Decibels::decibelsToGain (gainDb);
                band.gain += 0.02f * (target - band.gain);
                return input * band.gain;
            };

            band1Sample = processBand (band1, band1Sample, b1Thresh, b1Range);
            band2Sample = processBand (band2, band2Sample, b2Thresh, b2Range);

            const float processed = sample + band1Sample + band2Sample;
            data[i] = processed;
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

void DYNSmoothDestroyerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void DYNSmoothDestroyerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNSmoothDestroyerAudioProcessor::createParameterLayout()
{
    using AP = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<AP> ("band1_freq",   "Band1 Freq",
                                            juce::NormalisableRange<float> (40.0f, 8000.0f, 0.01f, 0.4f), 250.0f));
    params.push_back (std::make_unique<AP> ("band1_q",      "Band1 Q",
                                            juce::NormalisableRange<float> (0.1f, 10.0f, 0.001f, 0.5f), 1.2f));
    params.push_back (std::make_unique<AP> ("band1_thresh", "Band1 Thresh",
                                            juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f));
    params.push_back (std::make_unique<AP> ("band1_range",  "Band1 Range",
                                            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), -6.0f));
    params.push_back (std::make_unique<AP> ("band2_freq",   "Band2 Freq",
                                            juce::NormalisableRange<float> (200.0f, 18000.0f, 0.01f, 0.4f), 4000.0f));
    params.push_back (std::make_unique<AP> ("band2_q",      "Band2 Q",
                                            juce::NormalisableRange<float> (0.1f, 10.0f, 0.001f, 0.5f), 1.2f));
    params.push_back (std::make_unique<AP> ("band2_thresh", "Band2 Thresh",
                                            juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f));
    params.push_back (std::make_unique<AP> ("band2_range",  "Band2 Range",
                                            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), -6.0f));
    params.push_back (std::make_unique<AP> ("global_attack","Global Attack",
                                            juce::NormalisableRange<float> (1.0f, 200.0f, 0.01f, 0.3f), 15.0f));
    params.push_back (std::make_unique<AP> ("global_release","Global Release",
                                            juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f), 150.0f));
    params.push_back (std::make_unique<AP> ("mix",          "Mix",
                                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<AP> (kParamInput,    "Input Trim",
                                            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<AP> (kParamOutput,   "Output Trim",
                                            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNSmoothDestroyerAudioProcessorEditor::DYNSmoothDestroyerAudioProcessorEditor (DYNSmoothDestroyerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.SmoothDestroyer", "Smooth Destroyer")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto addSlider = [this](juce::Slider& s, const juce::String& label, bool macro = false) { initialiseSlider (s, label, macro); };

    addSlider (band1FreqSlider,   "B1 Freq", true);
    addSlider (band1QSlider,      "B1 Q");
    addSlider (band1ThreshSlider, "B1 Thresh");
    addSlider (band1RangeSlider,  "B1 Range");
    addSlider (band2FreqSlider,   "B2 Freq", true);
    addSlider (band2QSlider,      "B2 Q");
    addSlider (band2ThreshSlider, "B2 Thresh");
    addSlider (band2RangeSlider,  "B2 Range");
    addSlider (globalAttackSlider,"Attack");
    addSlider (globalReleaseSlider,"Release");
    addSlider (mixSlider,         "Mix");
    addSlider (inputTrimSlider,   "Input");
    addSlider (outputTrimSlider,  "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "band1_freq", "band1_q", "band1_thresh", "band1_range",
        "band2_freq", "band2_q", "band2_thresh", "band2_range",
        "global_attack", "global_release", "mix", kParamInput, kParamOutput
    };

    juce::Slider* sliders[] = {
        &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
        &band2FreqSlider, &band2QSlider, &band2ThreshSlider, &band2RangeSlider,
        &globalAttackSlider, &globalReleaseSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (940, 500);
}

void DYNSmoothDestroyerAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void DYNSmoothDestroyerAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNSmoothDestroyerAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
        &band2FreqSlider, &band2QSlider, &band2ThreshSlider, &band2RangeSlider,
        &globalAttackSlider, &globalReleaseSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void DYNSmoothDestroyerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNSmoothDestroyerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto bandWidth = top.getWidth() / 6;
    band1FreqSlider  .setBounds (top.removeFromLeft (bandWidth).reduced (8));
    band1QSlider     .setBounds (top.removeFromLeft (bandWidth).reduced (8));
    band1ThreshSlider.setBounds (top.removeFromLeft (bandWidth).reduced (8));
    band1RangeSlider .setBounds (top.removeFromLeft (bandWidth).reduced (8));
    band2FreqSlider  .setBounds (top.removeFromLeft (bandWidth).reduced (8));
    band2QSlider     .setBounds (top.removeFromLeft (bandWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 6;
    band2ThreshSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    band2RangeSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    globalAttackSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    globalReleaseSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    mixSlider         .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider   .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    outputTrimSlider.setBounds (footerComponent.getBounds().withSizeKeepingCentre (120, 48));
    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));

    layoutLabels();
}

juce::AudioProcessorEditor* DYNSmoothDestroyerAudioProcessor::createEditor()
{
    return new DYNSmoothDestroyerAudioProcessorEditor (*this);
}

void DYNSmoothDestroyerAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (band1States.size()) != requiredChannels)
    {
        band1States.resize (requiredChannels);
        band2States.resize (requiredChannels);

        juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize > 0 ? lastBlockSize : 512u, 1 };
        for (auto& band : band1States)
            band.bandFilter.prepare (spec);
        for (auto& band : band2States)
            band.bandFilter.prepare (spec);
    }
}

void DYNSmoothDestroyerAudioProcessor::updateBandCoefficients (DynamicBand& band, float freq, float q)
{
    if (currentSampleRate <= 0.0)
        return;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, freq, q);
    band.bandFilter.coefficients = coeffs;
}

float DYNSmoothDestroyerAudioProcessor::computeBandGain (float levelDb, float threshDb, float rangeDb) const
{
    if (levelDb < threshDb)
        return 0.0f;

    const float over = levelDb - threshDb;
    return juce::jlimit (-std::abs (rangeDb), std::abs (rangeDb), rangeDb * (over / 24.0f));
}

int DYNSmoothDestroyerAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNSmoothDestroyerAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNSmoothDestroyerAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNSmoothDestroyerAudioProcessor::applyPreset (int index)
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
    return new DYNSmoothDestroyerAudioProcessor();
}
