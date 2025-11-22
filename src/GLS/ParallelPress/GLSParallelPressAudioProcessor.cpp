#include "GLSParallelPressAudioProcessor.h"

namespace
{
float normaliseLogFreq (float value, float minHz, float maxHz)
{
    auto clamped = juce::jlimit (minHz, maxHz, value);
    const auto logMin = std::log10 (minHz);
    const auto logMax = std::log10 (maxHz);
    const auto logVal = std::log10 (clamped);
    return juce::jlimit (0.0f, 1.0f, (float) ((logVal - logMin) / (logMax - logMin)));
}
} // namespace

const std::array<GLSParallelPressAudioProcessor::Preset, 3> GLSParallelPressAudioProcessor::presetBank {{
    { "Drum Crush", {
        { "drive",        0.55f },
        { "comp_thresh", -22.0f },
        { "comp_ratio",   8.0f },
        { "attack",        5.0f },
        { "release",     140.0f },
        { "hpf_to_wet",   90.0f },
        { "lpf_to_wet", 12000.0f },
        { "wet_level",     2.0f },
        { "dry_level",    -2.0f },
        { "mix",           0.7f },
        { "input_trim",    0.0f },
        { "output_trim",   0.0f },
        { "auto_gain",     1.0f },
        { "ui_bypass",     0.0f }
    }},
    { "Vocal Glue", {
        { "drive",        0.35f },
        { "comp_thresh", -26.0f },
        { "comp_ratio",   3.5f },
        { "attack",        8.0f },
        { "release",     180.0f },
        { "hpf_to_wet",  120.0f },
        { "lpf_to_wet", 14000.0f },
        { "wet_level",     1.5f },
        { "dry_level",    -1.0f },
        { "mix",           0.65f },
        { "input_trim",    0.0f },
        { "output_trim",  -0.3f },
        { "auto_gain",     1.0f },
        { "ui_bypass",     0.0f }
    }},
    { "Bus Lift", {
        { "drive",        0.25f },
        { "comp_thresh", -18.0f },
        { "comp_ratio",   2.5f },
        { "attack",       12.0f },
        { "release",     220.0f },
        { "hpf_to_wet",   60.0f },
        { "lpf_to_wet", 16000.0f },
        { "wet_level",     0.5f },
        { "dry_level",     0.0f },
        { "mix",           0.55f },
        { "input_trim",    0.0f },
        { "output_trim",   0.0f },
        { "auto_gain",     0.0f },
        { "ui_bypass",     0.0f }
    }}
}};

GLSParallelPressAudioProcessor::GLSParallelPressAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARALLEL_PRESS", createParameterLayout())
{
}

void GLSParallelPressAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize();
    dryBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                       (int) lastBlockSize, false, false, true);
    wetBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                       (int) lastBlockSize, false, false, true);
    for (auto& state : channelStates)
    {
        state.hpf.reset();
        state.lpf.reset();
        state.envelope = 0.0f;
        state.gain = 1.0f;
    }
}

void GLSParallelPressAudioProcessor::releaseResources()
{
}

void GLSParallelPressAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const bool bypassed = apvts.getRawParameterValue ("ui_bypass")->load() > 0.5f;
    if (bypassed)
        return;

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto drive      = read ("drive");
    const auto thresh     = read ("comp_thresh");
    const auto ratio      = juce::jmax (1.0f, read ("comp_ratio"));
    const auto attack     = read ("attack");
    const auto release    = read ("release");
    const auto hpfWet     = read ("hpf_to_wet");
    const auto lpfWet     = read ("lpf_to_wet");
    const auto wetLevel   = read ("wet_level");
    const auto dryLevel   = read ("dry_level");
    const auto mix        = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputTrim  = juce::Decibels::decibelsToGain (read ("input_trim"));
    const auto outputTrim = juce::Decibels::decibelsToGain (read ("output_trim"));
    const bool autoGain   = apvts.getRawParameterValue ("auto_gain")->load() > 0.5f;

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureStateSize();
    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    wetBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);

    const auto attackCoeff  = std::exp (-1.0f / (juce::jmax (0.1f, attack) * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (juce::jmax (1.0f, release) * 0.001f * currentSampleRate));
    const auto threshDb     = thresh;
    const auto wetGain      = juce::Decibels::decibelsToGain (wetLevel);
    const auto dryGain      = juce::Decibels::decibelsToGain (dryLevel);

    const auto numSamples   = wetBuffer.getNumSamples();
    const auto numChannels  = wetBuffer.getNumChannels();
    float blockReductionDb  = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        updateFilterCoefficients (state, hpfWet, lpfWet);

        auto* wetData = wetBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = wetData[i];
            sample = state.hpf.processSample (sample);
            sample = state.lpf.processSample (sample);

            const auto level = std::abs (sample);
            auto& env = state.envelope;
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const auto levelDb = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f));
            const auto gainDb = computeCompressorGain (levelDb, threshDb, ratio);
            const auto targetGain = juce::Decibels::decibelsToGain (gainDb);
            state.gain += 0.02f * (targetGain - state.gain);

            blockReductionDb = juce::jmin (blockReductionDb, gainDb);

            sample = sample * state.gain;
            sample = applyDrive (sample, drive);

            wetData[i] = sample;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = wetBuffer.getWritePointer (ch);
        auto* dryData = dryBuffer.getWritePointer (ch);
        auto* outData = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            outData[i] = wetData[i] * wetGain + dryData[i] * dryGain;
    }

    if (autoGain)
    {
        const auto makeupDb = juce::jlimit (-12.0f, 12.0f, -blockReductionDb);
        const auto makeupGain = juce::Decibels::decibelsToGain (makeupDb);
        buffer.applyGain (makeupGain);
    }

    if (mix < 0.999f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* dryData = dryBuffer.getReadPointer (ch);
            auto* out = buffer.getWritePointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                out[i] = out[i] * mix + dryData[i] * (1.0f - mix);
        }
    }

    buffer.applyGain (outputTrim);
    lastReductionDb.store (blockReductionDb);
}

int GLSParallelPressAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GLSParallelPressAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GLSParallelPressAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GLSParallelPressAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSParallelPressAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSParallelPressAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange     = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto timeRange   = juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.3f);
    auto releaseRange= juce::NormalisableRange<float> (10.0f, 500.0f, 0.01f, 0.3f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive",        "Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_thresh",  "Comp Thresh", dBRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_ratio",   "Comp Ratio", juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",       "Attack", timeRange, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",      "Release", releaseRange, 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf_to_wet",   "HPF to Wet", juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lpf_to_wet",   "LPF to Wet", juce::NormalisableRange<float> (2000.0f, 20000.0f, 0.01f, 0.35f), 15000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("wet_level",    "Wet Level", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("dry_level",    "Dry Level", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim",   "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",  "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("auto_gain",    "Auto Gain", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("ui_bypass",    "Soft Bypass", false));

    return { params.begin(), params.end() };
}

class ParallelPressVisual : public juce::Component, private juce::Timer
{
public:
    ParallelPressVisual (GLSParallelPressAudioProcessor& proc,
                         juce::AudioProcessorValueTreeState& stateRef,
                         juce::Colour accentColour)
        : processor (proc), apvts (stateRef), accent (accentColour)
    {
        hpf   = apvts.getRawParameterValue ("hpf_to_wet");
        lpf   = apvts.getRawParameterValue ("lpf_to_wet");
        wet   = apvts.getRawParameterValue ("wet_level");
        dry   = apvts.getRawParameterValue ("dry_level");
        mix   = apvts.getRawParameterValue ("mix");
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (6.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.4f);

        auto meter = bounds.removeFromRight (64.0f).reduced (10.0f);
        drawGainReductionMeter (g, meter);

        auto freqArea = bounds.reduced (12.0f);
        drawFilterBand (g, freqArea);
        drawLabels (g, freqArea);
    }

private:
    GLSParallelPressAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;
    std::atomic<float>* hpf = nullptr;
    std::atomic<float>* lpf = nullptr;
    std::atomic<float>* wet = nullptr;
    std::atomic<float>* dry = nullptr;
    std::atomic<float>* mix = nullptr;

    void timerCallback() override { repaint(); }

    void drawGainReductionMeter (juce::Graphics& g, juce::Rectangle<float> meter)
    {
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (meter, 6.0f, 1.2f);

        const auto reductionDb = juce::jlimit (-30.0f, 0.0f, processor.getLastReductionDb());
        const auto norm = juce::jlimit (0.0f, 1.0f, -reductionDb / 30.0f);
        auto fill = meter.withHeight (meter.getHeight() * norm)
                          .withY (meter.getBottom() - meter.getHeight() * norm);
        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (fill, 6.0f);

        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (11.0f));
        g.drawFittedText (juce::String (juce::roundToInt (reductionDb)) + " dB",
                          meter.toNearestInt().translated (0, -18),
                          juce::Justification::centred, 1);
    }

    void drawFilterBand (juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto baseLine = juce::Line<float> (area.getX(), area.getBottom(), area.getRight(), area.getBottom());
        g.setColour (gls::ui::Colours::grid());
        g.drawLine (baseLine, 1.5f);
        g.drawLine ({ area.getX(), area.getY(), area.getX(), area.getBottom() }, 1.0f);

        const auto hpfVal = hpf != nullptr ? hpf->load() : 80.0f;
        const auto lpfVal = lpf != nullptr ? lpf->load() : 15000.0f;

        const auto hpfNorm = normaliseLogFreq (hpfVal, 20.0f, 20000.0f);
        const auto lpfNorm = normaliseLogFreq (lpfVal, 20.0f, 20000.0f);

        const auto hpfX = area.getX() + area.getWidth() * hpfNorm;
        const auto lpfX = area.getX() + area.getWidth() * lpfNorm;
        juce::Rectangle<float> passband (hpfX, area.getY(),
                                         juce::jmax (lpfX - hpfX, 8.0f),
                                         area.getHeight());
        g.setColour (accent.withAlpha (0.15f));
        g.fillRect (passband);

        g.setColour (accent);
        g.drawVerticalLine ((int) std::round (hpfX), area.getY(), area.getBottom());
        g.drawVerticalLine ((int) std::round (lpfX), area.getY(), area.getBottom());
    }

    void drawLabels (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (12.0f));
        juce::String info;
        info << "HPF " << juce::String (hpf != nullptr ? hpf->load() : 80.0f, 1) << " Hz\n";
        info << "LPF " << juce::String (lpf != nullptr ? lpf->load() : 15000.0f, 1) << " Hz\n";
        info << "Wet " << juce::String (wet != nullptr ? wet->load() : 0.0f, 1) << " dB | Dry "
             << juce::String (dry != nullptr ? dry->load() : 0.0f, 1) << " dB\n";
        info << "Mix " << juce::String ((mix != nullptr ? mix->load() : 1.0f) * 100.0f, 1) << " %";
        g.drawFittedText (info, area.toNearestInt(), juce::Justification::centredLeft, 4);
    }
};

GLSParallelPressAudioProcessorEditor::GLSParallelPressAudioProcessorEditor (GLSParallelPressAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.ParallelPress", "Parallel Press")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<ParallelPressVisual> (processorRef, processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (driveSlider,      "Drive",        true);
    configureSlider (compThreshSlider, "Threshold",    true);
    configureSlider (compRatioSlider,  "Ratio",        true);
    configureSlider (attackSlider,     "Attack",       true);
    configureSlider (releaseSlider,    "Release",      true);

    configureSlider (hpfWetSlider,     "HPF Wet",      false);
    configureSlider (lpfWetSlider,     "LPF Wet",      false);
    configureSlider (wetLevelSlider,   "Wet Level",    false);
    configureSlider (dryLevelSlider,   "Dry Level",    false);

    configureSlider (inputTrimSlider,  "Input",        false, true);
    configureSlider (mixSlider,        "Dry / Wet",    false, true);
    configureSlider (outputTrimSlider, "Output",       false, true);

    configureToggle (autoGainButton);
    configureToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, id, slider));
    };

    attachSlider ("drive",        driveSlider);
    attachSlider ("comp_thresh",  compThreshSlider);
    attachSlider ("comp_ratio",   compRatioSlider);
    attachSlider ("attack",       attackSlider);
    attachSlider ("release",      releaseSlider);
    attachSlider ("hpf_to_wet",   hpfWetSlider);
    attachSlider ("lpf_to_wet",   lpfWetSlider);
    attachSlider ("wet_level",    wetLevelSlider);
    attachSlider ("dry_level",    dryLevelSlider);
    attachSlider ("input_trim",   inputTrimSlider);
    attachSlider ("mix",          mixSlider);
    attachSlider ("output_trim",  outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "auto_gain", autoGainButton));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (960, 540);
}

GLSParallelPressAudioProcessorEditor::~GLSParallelPressAudioProcessorEditor()
{
    autoGainButton.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSParallelPressAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name,
                                                            bool isMacro, bool isLinear)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (isLinear ? juce::Slider::LinearHorizontal
                                    : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 72 : 64, 20);
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

void GLSParallelPressAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GLSParallelPressAudioProcessorEditor::layoutLabels()
{
    for (auto& pair : labeledSliders)
    {
        if (pair.slider == nullptr || pair.label == nullptr)
            continue;

        auto sliderBounds = pair.slider->getBounds();
        auto labelBounds = sliderBounds.withHeight (18).translated (0, -20);
        pair.label->setBounds (labelBounds);
    }
}

void GLSParallelPressAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void GLSParallelPressAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.34f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.27f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 5;
    driveSlider     .setBounds (left.removeFromTop (macroHeight).reduced (8));
    compThreshSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    compRatioSlider .setBounds (left.removeFromTop (macroHeight).reduced (8));
    attackSlider    .setBounds (left.removeFromTop (macroHeight).reduced (8));
    releaseSlider   .setBounds (left.removeFromTop (macroHeight).reduced (8));

    auto topRow = right.removeFromTop (right.getHeight() / 2);
    hpfWetSlider.setBounds (topRow.removeFromLeft (topRow.getWidth() / 2).reduced (8));
    lpfWetSlider.setBounds (topRow.reduced (8));

    auto bottomRow = right.removeFromTop (juce::jmax (right.getHeight() - 44, 0));
    wetLevelSlider.setBounds (bottomRow.removeFromLeft (bottomRow.getWidth() / 2).reduced (8));
    dryLevelSlider.setBounds (bottomRow.reduced (8));
    autoGainButton.setBounds (right.reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    mixSlider       .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* GLSParallelPressAudioProcessor::createEditor()
{
    return new GLSParallelPressAudioProcessorEditor (*this);
}

void GLSParallelPressAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (requiredChannels <= 0)
    {
        channelStates.clear();
        return;
    }

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };

    const auto previous = (int) channelStates.size();
    if (previous < requiredChannels)
    {
        channelStates.resize (requiredChannels);
        for (int ch = previous; ch < requiredChannels; ++ch)
        {
            channelStates[ch].hpf.prepare (spec);
            channelStates[ch].hpf.reset();
            channelStates[ch].lpf.prepare (spec);
            channelStates[ch].lpf.reset();
            channelStates[ch].envelope = 0.0f;
            channelStates[ch].gain = 1.0f;
        }
    }

    for (auto& state : channelStates)
    {
        state.hpf.prepare (spec);
        state.lpf.prepare (spec);
    }
}

void GLSParallelPressAudioProcessor::updateFilterCoefficients (ChannelState& state, float hpfFreq, float lpfFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                        juce::jlimit (10.0f, (float) (currentSampleRate * 0.45), hpfFreq));
    const auto lpf = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                       juce::jlimit (100.0f, (float) (currentSampleRate * 0.49), lpfFreq));

    state.hpf.coefficients = hpf;
    state.lpf.coefficients = lpf;
}

float GLSParallelPressAudioProcessor::computeCompressorGain (float levelDb, float threshDb, float ratio) const
{
    if (levelDb <= threshDb)
        return 0.0f;

    const auto over = levelDb - threshDb;
    const auto reduced = over / ratio;
    return threshDb + reduced - levelDb;
}

float GLSParallelPressAudioProcessor::applyDrive (float sample, float drive)
{
    if (drive <= 0.0f)
        return sample;

    const auto driveAmount = juce::jmap (drive, 1.0f, 8.0f);
    const auto saturated = std::tanh (sample * driveAmount);
    return juce::jmap (drive, sample, saturated);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSParallelPressAudioProcessor();
}

void GLSParallelPressAudioProcessor::applyPreset (int index)
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
