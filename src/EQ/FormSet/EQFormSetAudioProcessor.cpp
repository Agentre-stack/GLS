#include "EQFormSetAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "FORM_SET";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<EQFormSetAudioProcessor::Preset, 3> EQFormSetAudioProcessor::presetBank {{
    { "Vocal Morph", {
        { "formant_freq",   900.0f },
        { "formant_width",    0.6f },
        { "movement",         0.35f },
        { "intensity",        0.5f },
        { "mix",              0.75f },
        { kParamInput,        0.0f },
        { kParamOutput,      -0.5f },
        { kParamBypass,       0.0f }
    }},
    { "Guitar Talk", {
        { "formant_freq",  1200.0f },
        { "formant_width",   0.8f },
        { "movement",        0.6f },
        { "intensity",       0.6f },
        { "mix",             0.7f },
        { kParamInput,       0.0f },
        { kParamOutput,     -1.0f },
        { kParamBypass,      0.0f }
    }},
    { "FX Drone", {
        { "formant_freq",   500.0f },
        { "formant_width",    1.2f },
        { "movement",         0.8f },
        { "intensity",        0.8f },
        { "mix",              0.6f },
        { kParamInput,        0.0f },
        { kParamOutput,     -2.0f },
        { kParamBypass,       0.0f }
    }}
}};

EQFormSetAudioProcessor::EQFormSetAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void EQFormSetAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& formant : formantFilters)
    {
        formant.filter.prepare (spec);
        formant.filter.reset();
        formant.phase = 0.0f;
    }
}

void EQFormSetAudioProcessor::releaseResources()
{
}

void EQFormSetAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float formantFreq  = get ("formant_freq");
    const float formantWidth = juce::jlimit (0.1f, 2.0f, get ("formant_width"));
    const float movement     = juce::jlimit (0.0f, 1.0f, get ("movement"));
    const float intensity    = juce::jlimit (0.0f, 1.0f, get ("intensity"));
    const float mix          = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float inputGain    = juce::Decibels::decibelsToGain (get (kParamInput));
    const float outputGain   = juce::Decibels::decibelsToGain (get (kParamOutput));
    const bool bypassed      = get (kParamBypass) > 0.5f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    updateFormantFilters (formantFreq, formantWidth, movement);

    const float modulationDepth = juce::jmap (movement, 0.0f, 300.0f);
    const float intensityGainDb = juce::jmap (intensity, 0.0f, 12.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* dry  = dryBuffer.getReadPointer (ch);
        auto& formant = formantFilters[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float mod = std::sin (formant.phase);
            const float modInput = data[i] + mod * 0.02f;
            const float modulated = formant.filter.processSample (modInput);
            const float enhanced = modulated * juce::Decibels::decibelsToGain (intensityGainDb * std::abs (mod));
            data[i] = (enhanced * mix + dry[i] * (1.0f - mix)) * outputGain;

            formant.phase += (juce::MathConstants<float>::twoPi * (formantFreq + modulationDepth * mod))
                             / static_cast<float> (currentSampleRate);
            if (formant.phase > juce::MathConstants<float>::twoPi)
                formant.phase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void EQFormSetAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void EQFormSetAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQFormSetAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant_freq",  "Formant Freq",
                                                                   juce::NormalisableRange<float> (200.0f, 4000.0f, 0.01f, 0.4f), 800.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant_width", "Formant Width",
                                                                   juce::NormalisableRange<float> (0.1f, 2.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("movement",     "Movement",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("intensity",    "Intensity",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,    "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput,   "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass,   "Soft Bypass", false));

    return { params.begin(), params.end() };
}

EQFormSetAudioProcessorEditor::EQFormSetAudioProcessorEditor (EQFormSetAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("EQ")),
      headerComponent ("EQ.FormSet", "Form Set")
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

    make (formantFreqSlider,  "Formant Freq", true);
    make (formantWidthSlider, "Formant Width", true);
    make (movementSlider,     "Movement");
    make (intensitySlider,    "Intensity");
    make (mixSlider,          "Mix");
    make (inputTrimSlider,    "Input");
    make (outputTrimSlider,   "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "formant_freq", "formant_width", "movement", "intensity", "mix", kParamInput, kParamOutput };
    juce::Slider* sliders[]      = { &formantFreqSlider, &formantWidthSlider, &movementSlider, &intensitySlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (760, 420);
}

EQFormSetAudioProcessorEditor::~EQFormSetAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void EQFormSetAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void EQFormSetAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void EQFormSetAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &formantFreqSlider, &formantWidthSlider, &movementSlider, &intensitySlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

void EQFormSetAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void EQFormSetAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    const int topWidth = top.getWidth() / 3;
    formantFreqSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    formantWidthSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));
    movementSlider    .setBounds (top.removeFromLeft (topWidth).reduced (8));

    const int bottomWidth = bottom.getWidth() / 4;
    intensitySlider   .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    mixSlider         .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider   .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* EQFormSetAudioProcessor::createEditor()
{
    return new EQFormSetAudioProcessorEditor (*this);
}

void EQFormSetAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
    {
        formantFilters.clear();
        dryBuffer.setSize (0, 0);
        return;
    }

    const bool needsResize = (int) formantFilters.size() < numChannels;
    if (needsResize)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        const auto previous = (int) formantFilters.size();
        formantFilters.resize ((size_t) numChannels);
        for (int ch = previous; ch < numChannels; ++ch)
        {
            formantFilters[(size_t) ch].filter.prepare (spec);
            formantFilters[(size_t) ch].filter.reset();
            formantFilters[(size_t) ch].phase = 0.0f;
        }
    }

    dryBuffer.setSize (numChannels, (int) (lastBlockSize > 0 ? lastBlockSize : 512u), false, false, true);
}

void EQFormSetAudioProcessor::updateFormantFilters (float baseFreq, float width, float movement)
{
    if (currentSampleRate <= 0.0)
        return;

    const float freq = juce::jlimit (200.0f, (float) (currentSampleRate * 0.45f), baseFreq);
    const float bandwidth = juce::jlimit (0.2f, 5.0f, width * (1.0f + movement));
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, freq, bandwidth);
    for (auto& formant : formantFilters)
        formant.filter.coefficients = coeffs;
}

int EQFormSetAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String EQFormSetAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void EQFormSetAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void EQFormSetAudioProcessor::applyPreset (int index)
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
    return new EQFormSetAudioProcessor();
}
