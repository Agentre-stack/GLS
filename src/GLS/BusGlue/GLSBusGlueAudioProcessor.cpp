#include "GLSBusGlueAudioProcessor.h"

namespace
{
float normaliseLog (float value, float minHz, float maxHz)
{
    auto clamped = juce::jlimit (minHz, maxHz, value);
    const auto logMin = std::log10 (minHz);
    const auto logMax = std::log10 (maxHz);
    const auto logVal = std::log10 (clamped);
    return juce::jlimit (0.0f, 1.0f, (float) ((logVal - logMin) / (logMax - logMin)));
}
} // namespace

const std::array<GLSBusGlueAudioProcessor::Preset, 3> GLSBusGlueAudioProcessor::presetBank {{
    { "Drum Glue", {
        { "thresh",     -18.0f },
        { "ratio",        4.0f },
        { "attack",      10.0f },
        { "release",    120.0f },
        { "knee",         6.0f },
        { "sc_hpf",      80.0f },
        { "input_trim",   0.0f },
        { "mix",          0.75f },
        { "output",       0.0f },
        { "ui_bypass",    0.0f }
    }},
    { "MixBus Glue", {
        { "thresh",     -12.0f },
        { "ratio",        2.0f },
        { "attack",      30.0f },
        { "release",    200.0f },
        { "knee",         4.0f },
        { "sc_hpf",      60.0f },
        { "input_trim",  -1.0f },
        { "mix",          0.65f },
        { "output",       0.0f },
        { "ui_bypass",    0.0f }
    }},
    { "Vocal Bus", {
        { "thresh",     -20.0f },
        { "ratio",        3.0f },
        { "attack",      12.0f },
        { "release",    180.0f },
        { "knee",         8.0f },
        { "sc_hpf",     120.0f },
        { "input_trim",   0.0f },
        { "mix",          0.8f },
        { "output",       0.0f },
        { "ui_bypass",    0.0f }
    }}
}};

GLSBusGlueAudioProcessor::GLSBusGlueAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BUS_GLUE", createParameterLayout())
{
}

void GLSBusGlueAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec {
        currentSampleRate,
        lastBlockSize,
        1
    };

    sidechainFilter.prepare (spec);
    sidechainFilter.reset();
    detectorEnvelope = 0.0f;
    gainSmoothed = 1.0f;
}

void GLSBusGlueAudioProcessor::releaseResources()
{
}

void GLSBusGlueAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto readParam = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto bypassed  = apvts.getRawParameterValue ("ui_bypass")->load() > 0.5f;
    if (bypassed)
        return;

    const auto threshDb   = readParam ("thresh");
    const auto ratio      = juce::jmax (1.0f, readParam ("ratio"));
    const auto attackMs   = juce::jmax (0.1f, readParam ("attack"));
    const auto releaseMs  = juce::jmax (1.0f, readParam ("release"));
    const auto kneeDb     = juce::jmax (0.0f, readParam ("knee"));
    const auto scHpf      = readParam ("sc_hpf");
    const auto mix        = juce::jlimit (0.0f, 1.0f, readParam ("mix"));
    const auto inputTrim  = juce::Decibels::decibelsToGain (readParam ("input_trim"));
    const auto outputTrim = juce::Decibels::decibelsToGain (readParam ("output"));

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);
    updateSidechainFilter (scHpf);

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float scSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            scSample += buffer.getSample (ch, sample);
        scSample /= juce::jmax (1, numChannels);

        scSample = sidechainFilter.processSample (scSample);
        scSample = std::abs (scSample);

        if (scSample > detectorEnvelope)
            detectorEnvelope = attackCoeff * detectorEnvelope + (1.0f - attackCoeff) * scSample;
        else
            detectorEnvelope = releaseCoeff * detectorEnvelope + (1.0f - releaseCoeff) * scSample;

        const auto levelDb = juce::Decibels::gainToDecibels (juce::jmax (detectorEnvelope, 1.0e-6f));
        const auto gainDb  = computeGainDb (levelDb, threshDb, ratio, kneeDb);
        const auto targetGain = juce::Decibels::decibelsToGain (gainDb);
        gainSmoothed += 0.05f * (targetGain - gainSmoothed);
        lastReductionDb.store (juce::jlimit (-48.0f, 0.0f, juce::Decibels::gainToDecibels (gainSmoothed)),
                               std::memory_order_relaxed);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            data[sample] *= gainSmoothed;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }

    if (outputTrim != 1.0f)
        buffer.applyGain (outputTrim);
}

void GLSBusGlueAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void GLSBusGlueAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSBusGlueAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange      = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto timeRange    = juce::NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.25f);
    auto releaseRange = juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",  "Threshold", dBRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",   "Ratio", juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",  "Attack", timeRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release", releaseRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("knee",    "Knee", juce::NormalisableRange<float> (0.0f, 18.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sc_hpf",  "SC HPF", juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim", "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",     "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output",  "Output", juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

class BusGlueVisual : public juce::Component, private juce::Timer
{
public:
    BusGlueVisual (GLSBusGlueAudioProcessor& proc, juce::AudioProcessorValueTreeState& stateRef, juce::Colour accentColour)
        : processor (proc), state (stateRef), accent (accentColour)
    {
        thresh  = state.getRawParameterValue ("thresh");
        ratio   = state.getRawParameterValue ("ratio");
        attack  = state.getRawParameterValue ("attack");
        release = state.getRawParameterValue ("release");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.5f);

        auto meter = bounds.removeFromRight (52.0f).reduced (10.0f);
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (meter, 6.0f, 1.2f);

        const auto reductionDb = juce::jlimit (-30.0f, 0.0f, processor.getLastGainReductionDb());
        const float reductionNorm = juce::jlimit (0.0f, 1.0f, -reductionDb / 30.0f);
        auto fill = meter.withHeight (meter.getHeight() * reductionNorm).withY (meter.getBottom() - meter.getHeight() * reductionNorm);
        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (fill, 6.0f);

        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (11.0f));
        g.drawFittedText (juce::String (juce::roundToInt (reductionDb)) + juce::String (" dB"),
                          meter.toNearestInt().translated (0, -18), juce::Justification::centred, 1);

        auto textArea = bounds.reduced (16.0f);
        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (12.0f));
        auto info = juce::String ("Thresh ")
                      + juce::String (thresh != nullptr ? thresh->load() : -18.0f, 1) + " dB\n";
        info += "Ratio " + juce::String (ratio != nullptr ? ratio->load() : 4.0f, 2) + ":1\n";
        info += "Atk " + juce::String (attack != nullptr ? attack->load() : 10.0f, 1) + " ms  /  Rel "
                + juce::String (release != nullptr ? release->load() : 100.0f, 1) + " ms";
        g.drawFittedText (info, textArea.toNearestInt(), juce::Justification::centredLeft, 3);
    }

    void timerCallback() override { repaint(); }

private:
    GLSBusGlueAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    juce::Colour accent;
    std::atomic<float>* thresh = nullptr;
    std::atomic<float>* ratio = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
};

GLSBusGlueAudioProcessorEditor::GLSBusGlueAudioProcessorEditor (GLSBusGlueAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.BusGlue", "Bus Glue")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<BusGlueVisual> (processorRef, processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (threshSlider, "Threshold", true);
    configureSlider (ratioSlider,  "Ratio", true);
    configureSlider (attackSlider, "Attack", true);
    configureSlider (releaseSlider,"Release", true);
    configureSlider (kneeSlider,   "Knee", false);
    configureSlider (scHpfSlider,  "SC HPF", false);
    configureSlider (inputTrimSlider, "Input", false, true);
    configureSlider (dryWetSlider,    "Dry / Wet", false, true);
    configureSlider (outputTrimSlider,"Output", false, true);
    configureToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* paramId, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramId, slider));
    };

    attachSlider ("thresh", threshSlider);
    attachSlider ("ratio", ratioSlider);
    attachSlider ("attack", attackSlider);
    attachSlider ("release", releaseSlider);
    attachSlider ("knee", kneeSlider);
    attachSlider ("sc_hpf", scHpfSlider);
    attachSlider ("input_trim", inputTrimSlider);
    attachSlider ("mix", dryWetSlider);
    attachSlider ("output", outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (960, 540);
}

GLSBusGlueAudioProcessorEditor::~GLSBusGlueAudioProcessorEditor()
{
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSBusGlueAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name,
                                                      bool isMacro, bool isLinear)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (isLinear ? juce::Slider::LinearHorizontal
                                    : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 70 : 64, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::thumbColourId, accentColour);
    slider.setColour (juce::Slider::trackColourId, accentColour);
    addAndMakeVisible (slider);

    auto label = std::make_unique<juce::Label>();
    label->setText (name, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    labeledSliders.push_back ({ &slider, label.get() });
    sliderLabels.push_back (std::move (label));
}

void GLSBusGlueAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GLSBusGlueAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        auto labelBounds = sliderBounds.withHeight (18).translated (0, -20);
        entry.label->setBounds (labelBounds);
    }
}

void GLSBusGlueAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.3f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void GLSBusGlueAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.33f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.26f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 4;
    threshSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    ratioSlider .setBounds (left.removeFromTop (macroHeight).reduced (8));
    attackSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    releaseSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));

    auto rightHeight = right.getHeight() / 2;
    kneeSlider.setBounds (right.removeFromTop (rightHeight).reduced (8));
    scHpfSlider.setBounds (right.removeFromTop (rightHeight).reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    dryWetSlider    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* GLSBusGlueAudioProcessor::createEditor()
{
    return new GLSBusGlueAudioProcessorEditor (*this);
}

int GLSBusGlueAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GLSBusGlueAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GLSBusGlueAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}


void GLSBusGlueAudioProcessor::updateSidechainFilter (float frequency)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto freq = juce::jlimit (10.0f, (float) (currentSampleRate * 0.45), frequency);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, freq);
    sidechainFilter.coefficients = coeffs;
}

float GLSBusGlueAudioProcessor::computeGainDb (float inputLevelDb, float thresholdDb, float ratio, float kneeDb) const
{
    const float slope = 1.0f / ratio;

    if (kneeDb > 0.0f)
    {
        const float halfKnee = kneeDb * 0.5f;

        if (inputLevelDb <= thresholdDb - halfKnee)
            return 0.0f;

        if (inputLevelDb >= thresholdDb + halfKnee)
            return (thresholdDb - inputLevelDb) * (1.0f - slope);

        const float x = inputLevelDb - (thresholdDb - halfKnee);
        return - (1.0f - slope) * (x * x) / (2.0f * kneeDb);
    }

    if (inputLevelDb <= thresholdDb)
        return 0.0f;

    return (thresholdDb - inputLevelDb) * (1.0f - slope);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSBusGlueAudioProcessor();
}

void GLSBusGlueAudioProcessor::applyPreset (int index)
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
