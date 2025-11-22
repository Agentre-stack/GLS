#include "GLSChannelStripOneAudioProcessor.h"
#include <array>

GLSChannelStripOneAudioProcessor::GLSChannelStripOneAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHANNEL_STRIP_ONE", createParameterLayout())
{
}

void GLSChannelStripOneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto totalChannels = juce::jmax (2, getTotalNumOutputChannels());
    const auto blockSize = juce::jmax (1, samplesPerBlock);
    dryBuffer.setSize (totalChannels, blockSize);
    ensureStateSize();

    for (auto& state : channelStates)
    {
        state.lowShelf.reset();
        state.lowMidBell.reset();
        state.highMidBell.reset();
        state.highShelf.reset();
        state.gateEnvelope = 0.0f;
        state.gateGain = 1.0f;
        state.compEnvelope = 0.0f;
        state.compGain = 1.0f;
    }
}

void GLSChannelStripOneAudioProcessor::releaseResources()
{
}

void GLSChannelStripOneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    auto readParam = [this](const juce::String& id)
    {
        if (auto* param = apvts.getRawParameterValue (id))
            return param->load();
        return 0.0f;
    };

    const bool bypassed   = readParam ("ui_bypass") > 0.5f;
    if (bypassed)
        return;

    const auto gateThresh  = readParam ("gate_thresh");
    const auto gateRange   = readParam ("gate_range");
    const auto compThresh  = readParam ("comp_thresh");
    const auto compRatio   = juce::jmax (1.0f, readParam ("comp_ratio"));
    const auto compAttack  = juce::jmax (0.1f, readParam ("comp_attack"));
    const auto compRelease = juce::jmax (1.0f, readParam ("comp_release"));
    const auto lowGain     = readParam ("low_gain");
    const auto lowMidGain  = readParam ("low_mid_gain");
    const auto highMidGain = readParam ("high_mid_gain");
    const auto highGain    = readParam ("high_gain");
    const auto satAmount   = juce::jlimit (0.0f, 1.0f, readParam ("sat_amount"));
    const auto mix         = juce::jlimit (0.0f, 1.0f, readParam ("mix"));
    const auto inputTrim   = juce::Decibels::decibelsToGain (readParam ("input_trim"));
    const auto outputTrim  = juce::Decibels::decibelsToGain (readParam ("output_trim"));

    ensureStateSize();
    buffer.applyGain (inputTrim);
    dryBuffer.setSize (buffer.getNumChannels(), numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    const auto gateThresholdLinear = juce::Decibels::decibelsToGain (gateThresh);
    const auto gateAttenuation     = juce::Decibels::decibelsToGain (-juce::jmax (0.0f, gateRange));
    const auto compThresholdDb     = compThresh;
    const auto attackCoeff         = std::exp (-1.0f / ((compAttack * 0.001f) * currentSampleRate));
    const auto releaseCoeff        = std::exp (-1.0f / ((compRelease * 0.001f) * currentSampleRate));
    const auto gateEnvCoeff        = std::exp (-1.0f / (0.005f * currentSampleRate));

    const auto totalChannels = buffer.getNumChannels();

    for (int ch = 0; ch < totalChannels; ++ch)
    {
        auto& state = channelStates[ch];
        updateEqCoefficients (state, lowGain, lowMidGain, highMidGain, highGain);

        auto* data = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];

            // Gate
            const auto level = std::abs (sample);
            state.gateEnvelope = gateEnvCoeff * state.gateEnvelope + (1.0f - gateEnvCoeff) * level;
            const auto targetGateGain = state.gateEnvelope >= gateThresholdLinear ? 1.0f : gateAttenuation;
            state.gateGain += 0.002f * (targetGateGain - state.gateGain);
            sample *= state.gateGain;

            // Compressor
            const auto detector = std::abs (sample);
            auto& env = state.compEnvelope;
            if (detector > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * detector;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * detector;

            auto envDb = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f));
            float gainDb = 0.0f;
            if (envDb > compThresholdDb)
            {
                const auto over = envDb - compThresholdDb;
                const auto compressed = compThresholdDb + over / compRatio;
                gainDb = compressed - envDb;
            }

            const auto targetCompGain = juce::Decibels::decibelsToGain (gainDb);
            state.compGain += 0.01f * (targetCompGain - state.compGain);
            sample *= state.compGain;

            // 4-band EQ
            sample = state.lowShelf.processSample (sample);
            sample = state.lowMidBell.processSample (sample);
            sample = state.highMidBell.processSample (sample);
            sample = state.highShelf.processSample (sample);

            // Saturation
            sample = softClip (sample, satAmount);

            data[i] = sample;
        }

        // Mix
        for (int i = 0; i < numSamples; ++i)
            data[i] = data[i] * mix + dry[i] * (1.0f - mix);
    }

    buffer.applyGain (outputTrim);
}

void GLSChannelStripOneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSChannelStripOneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData (data, sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSChannelStripOneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange    = juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f);
    auto gainRange  = juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f);
    auto timeRange  = juce::NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.25f);
    auto trimRange  = juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gate_thresh",  "Gate Thresh", dBRange, -40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gate_range",   "Gate Range",  juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_thresh",  "Comp Thresh", dBRange, -20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_ratio",   "Comp Ratio",  juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_attack",  "Comp Attack", timeRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_release", "Comp Release", juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f), 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_gain",     "Low Gain",     gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_mid_gain", "LowMid Gain",  gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_mid_gain","HighMid Gain", gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_gain",    "High Gain",    gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sat_amount",   "Sat Amount",   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",          juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim",   "Input Trim",   trimRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",  "Output Trim",  trimRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("ui_bypass",    "Soft Bypass",  false));

    return { params.begin(), params.end() };
}

class ChannelStripVisual : public juce::Component, private juce::Timer
{
public:
    ChannelStripVisual (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
        : apvts (state), accent (accentColour)
    {
        auto fetch = [&state](const juce::String& id) -> std::atomic<float>*
        {
            return state.getRawParameterValue (id);
        };

        gateRange   = fetch ("gate_range");
        compRatio   = fetch ("comp_ratio");
        compThresh  = fetch ("comp_thresh");
        gateThresh  = fetch ("gate_thresh");
        satAmount   = fetch ("sat_amount");
        lowGain     = fetch ("low_gain");
        lowMidGain  = fetch ("low_mid_gain");
        highMidGain = fetch ("high_mid_gain");
        highGain    = fetch ("high_gain");

        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (gls::ui::Colours::panel());
        auto bounds = getLocalBounds().toFloat().reduced (12.0f);

        g.setColour (gls::ui::Colours::grid());
        for (int i = 1; i < 4; ++i)
        {
            const auto y = bounds.getY() + bounds.getHeight() * (float) i / 4.0f;
            g.drawLine (bounds.getX(), y, bounds.getRight(), y, 1.0f);
        }

        auto dynamicArea = bounds.removeFromTop (bounds.getHeight() * 0.35f).reduced (8.0f);
        auto gateArea = dynamicArea.removeFromLeft (dynamicArea.getWidth() * 0.48f).reduced (8.0f);
        auto compArea = dynamicArea.reduced (8.0f);

        auto drawMeter = [&g, this](juce::Rectangle<float> meterBounds, float value, const juce::String& label)
        {
            g.setColour (gls::ui::Colours::outline());
            g.drawRoundedRectangle (meterBounds, 6.0f, 1.5f);
            auto fill = meterBounds.withHeight (meterBounds.getHeight() * juce::jlimit (0.0f, 1.0f, value)).withBottom (meterBounds.getBottom());
            g.setColour (accent.withMultipliedAlpha (0.8f));
            g.fillRoundedRectangle (fill, 6.0f);
            g.setColour (gls::ui::Colours::textSecondary());
            g.setFont (gls::ui::makeFont (12.0f));
            g.drawFittedText (label, meterBounds.toNearestInt(), juce::Justification::centred, 1);
        };

        const auto gateValue = gateRange != nullptr ? std::abs (gateRange->load()) / 60.0f : 0.0f;
        const auto compValue = compRatio != nullptr ? juce::jlimit (0.0f, 1.0f, 1.0f / juce::jmax (1.0f, compRatio->load())) : 0.0f;
        drawMeter (gateArea, gateValue, "Gate");
        drawMeter (compArea, compValue, "Comp");

        auto eqArea = bounds.reduced (8.0f);
        auto bandWidth = eqArea.getWidth() / 4.0f;
        std::array<std::atomic<float>*, 4> eqParams { lowGain, lowMidGain, highMidGain, highGain };
        const juce::StringArray labels { "Low", "Low Mid", "High Mid", "High" };

        for (size_t i = 0; i < eqParams.size(); ++i)
        {
            auto bar = juce::Rectangle<float> (eqArea.getX() + (float) i * bandWidth + 8.0f,
                                               eqArea.getY(), bandWidth - 16.0f, eqArea.getHeight());
            g.setColour (gls::ui::Colours::outline());
            g.drawRect (bar);

            const auto value = eqParams[i] != nullptr ? (eqParams[i]->load() + 15.0f) / 30.0f : 0.5f;
            auto filled = bar.withHeight (bar.getHeight() * juce::jlimit (0.0f, 1.0f, value)).withBottom (bar.getBottom());
            g.setColour (accent.withMultipliedAlpha (0.7f));
            g.fillRect (filled);

            g.setColour (gls::ui::Colours::textSecondary());
            g.setFont (gls::ui::makeFont (12.0f));
            g.drawFittedText (labels[(int) i], bar.toNearestInt().withY (bar.getBottom() + 4).withHeight (16),
                              juce::Justification::centred, 1);
        }

        if (satAmount != nullptr)
        {
            auto sat = satAmount->load();
            auto satBounds = eqArea.removeFromTop (16.0f).translated (0.0f, -8.0f);
            g.setColour (gls::ui::Colours::textSecondary());
            g.drawFittedText ("Saturation", satBounds.toNearestInt(), juce::Justification::centredLeft, 1);
            auto satMeter = satBounds.withX (satBounds.getRight() - 120.0f).withWidth (110.0f).reduced (8.0f);
            g.setColour (gls::ui::Colours::outline());
            g.drawRect (satMeter);
            g.setColour (accent);
            g.fillRect (satMeter.withWidth (satMeter.getWidth() * juce::jlimit (0.0f, 1.0f, sat)));
        }
    }

    void timerCallback() override { repaint(); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;

    std::atomic<float>* gateRange = nullptr;
    std::atomic<float>* compRatio = nullptr;
    std::atomic<float>* compThresh = nullptr;
    std::atomic<float>* gateThresh = nullptr;
    std::atomic<float>* satAmount = nullptr;
    std::atomic<float>* lowGain = nullptr;
    std::atomic<float>* lowMidGain = nullptr;
    std::atomic<float>* highMidGain = nullptr;
    std::atomic<float>* highGain = nullptr;
};

GLSChannelStripOneAudioProcessorEditor::GLSChannelStripOneAudioProcessorEditor (GLSChannelStripOneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.ChannelStripOne", "Channel Strip One"),
      footerComponent()
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<ChannelStripVisual> (processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (gateThreshSlider,  "Gate Threshold", true);
    configureSlider (gateRangeSlider,   "Gate Range",     true);
    configureSlider (compThreshSlider,  "Comp Threshold", true);
    configureSlider (compRatioSlider,   "Comp Ratio",     true);
    configureSlider (compAttackSlider,  "Comp Attack",    false);
    configureSlider (compReleaseSlider, "Comp Release",   false);
    configureSlider (lowGainSlider,     "Low Gain",       false);
    configureSlider (lowMidGainSlider,  "Low Mid Gain",   false);
    configureSlider (highMidGainSlider, "High Mid Gain",  false);
    configureSlider (highGainSlider,    "High Gain",      false);
    configureSlider (satAmountSlider,   "Sat Amount",     false);
    configureSlider (dryWetSlider,      "Dry / Wet",      false, true);
    configureSlider (inputTrimSlider,   "Input",          false, true);
    configureSlider (outputTrimSlider,  "Output",         false, true);

    bypassButton.setButtonText ("Soft Bypass");
    bypassButton.setLookAndFeel (&lookAndFeel);
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* paramID, juce::Slider& slider)
    {
        attachments.push_back (std::make_unique<SliderAttachment> (state, paramID, slider));
    };

    attachSlider ("gate_thresh",  gateThreshSlider);
    attachSlider ("gate_range",   gateRangeSlider);
    attachSlider ("comp_thresh",  compThreshSlider);
    attachSlider ("comp_ratio",   compRatioSlider);
    attachSlider ("comp_attack",  compAttackSlider);
    attachSlider ("comp_release", compReleaseSlider);
    attachSlider ("low_gain",     lowGainSlider);
    attachSlider ("low_mid_gain", lowMidGainSlider);
    attachSlider ("high_mid_gain", highMidGainSlider);
    attachSlider ("high_gain",    highGainSlider);
    attachSlider ("sat_amount",   satAmountSlider);
    attachSlider ("mix",          dryWetSlider);
    attachSlider ("input_trim",   inputTrimSlider);
    attachSlider ("output_trim",  outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (960, 600);
}

GLSChannelStripOneAudioProcessorEditor::~GLSChannelStripOneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GLSChannelStripOneAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name,
                                                              bool isMacro, bool isLinear)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (isLinear ? juce::Slider::LinearHorizontal
                                    : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 70 : 60, 20);
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

void GLSChannelStripOneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());

    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.3f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void GLSChannelStripOneAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        auto labelBounds = sliderBounds.withHeight (18).translated (0, -22);
        entry.label->setBounds (labelBounds);
    }
}

void GLSChannelStripOneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.32f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.28f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 4;
    juce::Slider* macroSliders[] = { &gateThreshSlider, &gateRangeSlider, &compThreshSlider, &compRatioSlider };
    for (auto* slider : macroSliders)
    {
        auto slot = left.removeFromTop (macroHeight).reduced (8);
        slider->setBounds (slot);
    }

    auto microArea = right;
    auto rowHeight = microArea.getHeight() / 4;

    auto placeRow = [rowHeight](juce::Rectangle<int>& area, juce::Slider* first, juce::Slider* second)
    {
        auto row = area.removeFromTop (rowHeight);
        if (first != nullptr)
            first->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (8));
        if (second != nullptr)
            second->setBounds (row.reduced (8));
    };

    placeRow (microArea, &compAttackSlider, &compReleaseSlider);
    placeRow (microArea, &lowGainSlider, &lowMidGainSlider);
    placeRow (microArea, &highMidGainSlider, &highGainSlider);

    auto satRow = microArea.removeFromTop (rowHeight).reduced (8);
    satAmountSlider.setBounds (satRow);

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;

    auto slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    inputTrimSlider.setBounds (slot);

    slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    dryWetSlider.setBounds (slot);

    slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    outputTrimSlider.setBounds (slot);

    slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    bypassButton.setBounds (slot);

    layoutLabels();
}

juce::AudioProcessorEditor* GLSChannelStripOneAudioProcessor::createEditor()
{
    return new GLSChannelStripOneAudioProcessorEditor (*this);
}

void GLSChannelStripOneAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}

void GLSChannelStripOneAudioProcessor::updateEqCoefficients (ChannelState& state,
                                                             float lowGain, float lowMidGain,
                                                             float highMidGain, float highGain)
{
    const auto lowShelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 120.0f, 0.707f,
                                                                                   juce::Decibels::decibelsToGain (lowGain));
    const auto lowMidCoeffs   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 400.0f, 0.9f,
                                                                                     juce::Decibels::decibelsToGain (lowMidGain));
    const auto highMidCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 3000.0f, 0.9f,
                                                                                     juce::Decibels::decibelsToGain (highMidGain));
    const auto highShelfCoeffs= juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 8000.0f, 0.707f,
                                                                                    juce::Decibels::decibelsToGain (highGain));

    state.lowShelf.coefficients  = lowShelfCoeffs;
    state.lowMidBell.coefficients= lowMidCoeffs;
    state.highMidBell.coefficients= highMidCoeffs;
    state.highShelf.coefficients = highShelfCoeffs;
}

float GLSChannelStripOneAudioProcessor::softClip (float input, float amount)
{
    if (amount <= 0.0f)
        return input;

    const auto drive = juce::jmap (amount, 1.0f, 6.0f);
    const auto saturated = std::tanh (input * drive);
    return juce::jmap (amount, input, saturated);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSChannelStripOneAudioProcessor();
}
