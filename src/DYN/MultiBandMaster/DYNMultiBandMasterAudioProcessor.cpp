#include "DYNMultiBandMasterAudioProcessor.h"

namespace
{
constexpr auto kStateId      = "MULTIBAND_MASTER";
constexpr auto kParamBypass  = "ui_bypass";
constexpr auto kParamInput   = "input_trim";
constexpr auto kParamOutput  = "output_trim";
}

const std::array<DYNMultiBandMasterAudioProcessor::Preset, 3> DYNMultiBandMasterAudioProcessor::presetBank {{
    { "Master Gentle", {
        { "band1_freq",   120.0f },
        { "band2_freq",   650.0f },
        { "band3_freq",  3200.0f },
        { "band1_thresh",-24.0f },
        { "band2_thresh",-18.0f },
        { "band3_thresh",-14.0f },
        { "band1_ratio",   2.0f },
        { "band2_ratio",   2.3f },
        { "band3_ratio",   2.5f },
        { "mix",           0.85f },
        { kParamInput,     0.0f },
        { kParamOutput,    0.5f },
        { kParamBypass,    0.0f }
    }},
    { "Mix Glue", {
        { "band1_freq",   140.0f },
        { "band2_freq",   900.0f },
        { "band3_freq",  4500.0f },
        { "band1_thresh",-20.0f },
        { "band2_thresh",-16.0f },
        { "band3_thresh",-12.0f },
        { "band1_ratio",   2.4f },
        { "band2_ratio",   2.8f },
        { "band3_ratio",   3.0f },
        { "mix",           0.78f },
        { kParamInput,     0.0f },
        { kParamOutput,    0.0f },
        { kParamBypass,    0.0f }
    }},
    { "Vocal Pop", {
        { "band1_freq",   150.0f },
        { "band2_freq",  1200.0f },
        { "band3_freq",  5200.0f },
        { "band1_thresh",-26.0f },
        { "band2_thresh",-18.0f },
        { "band3_thresh",-10.0f },
        { "band1_ratio",   1.8f },
        { "band2_ratio",   2.5f },
        { "band3_ratio",   3.2f },
        { "mix",           0.88f },
        { kParamInput,     0.0f },
        { kParamOutput,    0.5f },
        { kParamBypass,    0.0f }
    }}
}};

DYNMultiBandMasterAudioProcessor::DYNMultiBandMasterAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNMultiBandMasterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureBandStateSize (getTotalNumOutputChannels());
}

void DYNMultiBandMasterAudioProcessor::releaseResources()
{
}

void DYNMultiBandMasterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    std::array<float, 3> freqs {
        read ("band1_freq"),
        read ("band2_freq"),
        read ("band3_freq")
    };

    std::array<float, 3> thresholds {
        read ("band1_thresh"),
        read ("band2_thresh"),
        read ("band3_thresh")
    };

    std::array<float, 3> ratios {
        juce::jmax (1.0f, read ("band1_ratio")),
        juce::jmax (1.0f, read ("band2_ratio")),
        juce::jmax (1.0f, read ("band3_ratio"))
    };

    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputDb   = read (kParamInput);
    const auto outputDb  = read (kParamOutput);
    const auto inputGain = juce::Decibels::decibelsToGain (inputDb);
    const auto outputGain= juce::Decibels::decibelsToGain (outputDb);

    buffer.applyGain (inputGain);
    dryBuffer.makeCopyOf (buffer, true);

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureBandStateSize (buffer.getNumChannels());
    updateBandFilters (freqs);

    const float attackMs  = 8.0f;
    const float releaseMs = 120.0f;
    const float attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * (float) currentSampleRate));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = data[i];
            float sample = drySample;

            for (size_t band = 0; band < bandStates.size(); ++band)
            {
                auto& state = bandStates[band][ch];
                const float bandSample = state.filter.processSample (drySample);
                const float level = std::abs (bandSample) + 1.0e-6f;

                if (level > state.envelope)
                    state.envelope = attackCoeff * state.envelope + (1.0f - attackCoeff) * level;
                else
                    state.envelope = releaseCoeff * state.envelope + (1.0f - releaseCoeff) * level;

                const float envDb = juce::Decibels::gainToDecibels (state.envelope);
                const float gain  = computeCompressorGain (envDb, thresholds[band], ratios[band]);
                state.gain += 0.02f * (gain - state.gain);

                const float processedBand = bandSample * state.gain;
                sample += (processedBand - bandSample);
            }

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

void DYNMultiBandMasterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNMultiBandMasterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNMultiBandMasterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto freqRange  = juce::NormalisableRange<float> (60.0f, 8000.0f, 0.01f, 0.4f);
    auto threshRange= juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_freq", "Band1 Freq", freqRange, 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_freq", "Band2 Freq", freqRange, 800.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band3_freq", "Band3 Freq", freqRange, 3200.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_thresh", "Band1 Thresh", threshRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_thresh", "Band2 Thresh", threshRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band3_thresh", "Band3 Thresh", threshRange, -12.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_ratio", "Band1 Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f, 0.5f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_ratio", "Band2 Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f, 0.5f), 2.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band3_ratio", "Band3 Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f, 0.5f), 3.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput, "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNMultiBandMasterAudioProcessorEditor::DYNMultiBandMasterAudioProcessorEditor (DYNMultiBandMasterAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.MultiBandMaster", "MultiBand Master")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    for (size_t i = 0; i < freqSliders.size(); ++i)
    {
        initSlider (freqSliders[i], juce::String ("Freq " + juce::String ((int) i + 1)), true);
        initSlider (threshSliders[i], juce::String ("Thresh " + juce::String ((int) i + 1)), true);
        initSlider (ratioSliders[i], juce::String ("Ratio " + juce::String ((int) i + 1)));
    }
    initSlider (mixSlider,       "Mix");
    initSlider (inputTrimSlider, "Input");
    initSlider (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "band1_freq", "band2_freq", "band3_freq",
        "band1_thresh", "band2_thresh", "band3_thresh",
        "band1_ratio", "band2_ratio", "band3_ratio",
        "mix", kParamInput, kParamOutput
    };

    std::vector<juce::Slider*> sliders = {
        &freqSliders[0], &freqSliders[1], &freqSliders[2],
        &threshSliders[0], &threshSliders[1], &threshSliders[2],
        &ratioSliders[0], &ratioSliders[1], &ratioSliders[2],
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[(int) i], *sliders[i]));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (900, 460);
}

void DYNMultiBandMasterAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (label);
    addAndMakeVisible (slider);

    auto labelPtr = std::make_unique<juce::Label>();
    labelPtr->setText (label, juce::dontSendNotification);
    labelPtr->setJustificationType (juce::Justification::centred);
    labelPtr->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    labelPtr->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*labelPtr);
    labels.push_back (std::move (labelPtr));
}

void DYNMultiBandMasterAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNMultiBandMasterAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders = {
        &freqSliders[0], &freqSliders[1], &freqSliders[2],
        &threshSliders[0], &threshSliders[1], &threshSliders[2],
        &ratioSliders[0], &ratioSliders[1], &ratioSliders[2],
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void DYNMultiBandMasterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNMultiBandMasterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto bandArea = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.65f));

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (bandArea.removeFromTop (bandArea.getHeight() / 3), { &freqSliders[0], &freqSliders[1], &freqSliders[2] });
    layoutRow (bandArea.removeFromTop (bandArea.getHeight() / 2), { &threshSliders[0], &threshSliders[1], &threshSliders[2] });
    layoutRow (bandArea, { &ratioSliders[0], &ratioSliders[1], &ratioSliders[2] });

    auto bottom = area;
    auto slot = bottom.getWidth() / 3;
    mixSlider.setBounds (bottom.removeFromLeft (slot).reduced (8));
    inputTrimSlider.setBounds (bottom.removeFromLeft (slot).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (slot).reduced (8));
    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));

    layoutLabels();
}

juce::AudioProcessorEditor* DYNMultiBandMasterAudioProcessor::createEditor()
{
    return new DYNMultiBandMasterAudioProcessorEditor (*this);
}

void DYNMultiBandMasterAudioProcessor::ensureBandStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };

    for (auto& bandVector : bandStates)
    {
        const int currentSize = (int) bandVector.size();
        if (currentSize < numChannels)
        {
            bandVector.resize (numChannels);
            for (int ch = currentSize; ch < numChannels; ++ch)
            {
                bandVector[ch].filter.prepare (spec);
                bandVector[ch].filter.reset();
                bandVector[ch].envelope = 0.0f;
                bandVector[ch].gain = 1.0f;
            }
        }

        for (auto& state : bandVector)
            state.filter.prepare (spec);
    }
}

void DYNMultiBandMasterAudioProcessor::updateBandFilters (const std::array<float, 3>& freqs)
{
    if (currentSampleRate <= 0.0)
        return;

    for (size_t band = 0; band < bandStates.size(); ++band)
    {
        const auto freq = juce::jlimit (40.0f, (float) (currentSampleRate * 0.45), freqs[band]);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, freq, 0.8f + (float) band * 0.4f);

        for (auto& state : bandStates[band])
            state.filter.coefficients = coeffs;
    }
}

float DYNMultiBandMasterAudioProcessor::computeCompressorGain (float levelDb, float threshDb, float ratio) const
{
    if (ratio <= 1.0f || levelDb <= threshDb)
        return 1.0f;

    const float over = levelDb - threshDb;
    const float compressed = threshDb + over / ratio;
    return juce::Decibels::decibelsToGain (compressed - levelDb);
}

int DYNMultiBandMasterAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNMultiBandMasterAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNMultiBandMasterAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNMultiBandMasterAudioProcessor::applyPreset (int index)
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
    return new DYNMultiBandMasterAudioProcessor();
}
