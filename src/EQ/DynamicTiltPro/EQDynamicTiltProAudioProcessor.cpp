#include "EQDynamicTiltProAudioProcessor.h"

const std::array<EQDynamicTiltProAudioProcessor::Preset, 3> EQDynamicTiltProAudioProcessor::presetBank {{
    { "Vocal Pop", {
        { "tilt",        3.0f },
        { "range",       2.5f },
        { "thresh",     -28.0f },
        { "pivot_freq", 2200.0f },
        { "attack",      25.0f },
        { "release",    180.0f },
        { "input_trim",   0.0f },
        { "mix",          0.85f },
        { "output_trim", -0.5f },
        { "detector_mode", 0.0f }, // Peak
        { "shelf_style",   2.0f }, // Tight
        { "ui_bypass",     0.0f }
    }},
    { "Drum Bus", {
        { "tilt",        1.5f },
        { "range",       4.0f },
        { "thresh",     -22.0f },
        { "pivot_freq",  900.0f },
        { "attack",      10.0f },
        { "release",    120.0f },
        { "input_trim",   0.0f },
        { "mix",          0.9f },
        { "output_trim",  0.0f },
        { "detector_mode", 1.0f }, // RMS
        { "shelf_style",   1.0f }, // Wide
        { "ui_bypass",     0.0f }
    }},
    { "Master Air", {
        { "tilt",        2.5f },
        { "range",       1.5f },
        { "thresh",     -18.0f },
        { "pivot_freq", 4500.0f },
        { "attack",      60.0f },
        { "release",    320.0f },
        { "input_trim",   0.0f },
        { "mix",          0.7f },
        { "output_trim", -0.4f },
        { "detector_mode", 1.0f }, // RMS
        { "shelf_style",   0.0f }, // Classic
        { "ui_bypass",     0.0f }
    }}
}};

EQDynamicTiltProAudioProcessor::EQDynamicTiltProAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DYNAMIC_TILT_PRO", createParameterLayout())
{
}

void EQDynamicTiltProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
    dryBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                       (int) lastBlockSize, false, false, true);

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    auto prepareVector = [&](auto& vec)
    {
        for (auto& filter : vec)
        {
            filter.prepare (spec);
            filter.reset();
        }
    };

    prepareVector (lowShelves);
    prepareVector (highShelves);
    std::fill (envelopes.begin(), envelopes.end(), 0.0f);
}

void EQDynamicTiltProAudioProcessor::releaseResources()
{
}

void EQDynamicTiltProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto tiltDb     = get ("tilt");
    const auto pivotFreq  = get ("pivot_freq");
    const auto threshDb   = get ("thresh");
    const auto rangeDb    = get ("range");
    const auto attackMs   = get ("attack");
    const auto releaseMs  = get ("release");
    const auto outputTrim = get ("output_trim");
    const auto mix        = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const auto inputTrim  = juce::Decibels::decibelsToGain (get ("input_trim"));
    const int detectorMode = (int) std::round (apvts.getRawParameterValue ("detector_mode")->load());
    const int styleIndex   = (int) std::round (apvts.getRawParameterValue ("shelf_style")->load());

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);

    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);

    const float attackSeconds  = juce::jmax (1.0f, attackMs) * 0.001f;
    const float releaseSeconds = juce::jmax (5.0f, releaseMs) * 0.001f;
    const float attackCoeff  = std::exp (-1.0f / (attackSeconds  * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (releaseSeconds * (float) currentSampleRate));
    const bool useRmsDetector = detectorMode == 1;

    float combinedEnv = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getReadPointer (ch);
        auto& env = envelopes[ch];
        for (int i = 0; i < numSamples; ++i)
        {
            float level = std::abs (data[i]) + 1.0e-6f;
            if (useRmsDetector)
                level = level * level;

            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const float magnitude = useRmsDetector ? std::sqrt (juce::jmax (env, 1.0e-8f))
                                                   : env;
            combinedEnv = juce::jmax (combinedEnv, magnitude);
        }
    }

    const float envDb = juce::Decibels::gainToDecibels (juce::jmax (combinedEnv, 1.0e-6f));
    const float normalized = juce::jlimit (-1.0f, 1.0f, (envDb - threshDb) / 24.0f);
    const float dynamicComponent = normalized * rangeDb;
    const float totalTilt = tiltDb + dynamicComponent;

    float shelfQ = 0.707f;
    if (styleIndex == 1)
        shelfQ = 0.5f;
    else if (styleIndex == 2)
        shelfQ = 1.2f;

    updateFilters (totalTilt, pivotFreq, shelfQ);

    currentTilt.store (totalTilt);
    lastEnvelopeDb.store (envDb);
    lastThresholdDb.store (threshDb);

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
        lowShelves[ch].process (ctx);
        highShelves[ch].process (ctx);
    }

    if (mix < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* dry = dryBuffer.getReadPointer (ch);
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = data[i] * mix + dry[i] * (1.0f - mix);
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (outputTrim));
}

int EQDynamicTiltProAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String EQDynamicTiltProAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void EQDynamicTiltProAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void EQDynamicTiltProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQDynamicTiltProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQDynamicTiltProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt",      "Tilt",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pivot_freq","Pivot Freq",
                                                                   juce::NormalisableRange<float> (150.0f, 6000.0f, 0.01f, 0.4f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",    "Threshold",
                                                                   juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("range",     "Range",
                                                                   juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",    "Attack",
                                                                   juce::NormalisableRange<float> (1.0f, 200.0f, 0.01f, 0.35f), 15.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",   "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 1000.0f, 0.01f, 0.35f), 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim","Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",       "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim","Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("detector_mode", "Detector",
                                                                    juce::StringArray { "Peak", "RMS" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("shelf_style", "Shelf Style",
                                                                    juce::StringArray { "Classic", "Wide", "Tight" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

struct TiltVisualComponent : public juce::Component, private juce::Timer
{
    TiltVisualComponent (EQDynamicTiltProAudioProcessor& proc, juce::Colour accentColour)
        : processor (proc), accent (accentColour)
    {
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (6.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.4f);

        auto curveArea = bounds.reduced (12.0f);
        drawTiltCurve (g, curveArea);
        drawEnvelopeMeter (g, bounds.removeFromBottom (32.0f));
    }

private:
    EQDynamicTiltProAudioProcessor& processor;
    juce::Colour accent;

    void drawTiltCurve (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float tiltDb = processor.getCurrentTiltDb();
        const float pivotNorm = 0.5f;

        auto mapDbToY = [area](float db)
        {
            const float norm = juce::jlimit (-18.0f, 18.0f, db) / 36.0f;
            return area.getCentreY() - norm * area.getHeight() * 0.8f;
        };

        juce::Path path;
        auto leftY = mapDbToY (tiltDb * -0.5f);
        auto rightY = mapDbToY (tiltDb * 0.5f);
        auto pivotX = area.getX() + area.getWidth() * pivotNorm;

        path.startNewSubPath (area.getX(), leftY);
        path.quadraticTo (pivotX, area.getCentreY(), area.getRight(), rightY);

        g.setColour (accent.withAlpha (0.15f));
        juce::Path fill;
        juce::PathStrokeType (6.0f).createStrokedPath (fill, path);
        g.fillPath (fill);

        g.setColour (accent);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    void drawEnvelopeMeter (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float envDb = processor.getEnvelopeDb();
        const float threshDb = processor.getThresholdDb();

        g.setColour (gls::ui::Colours::grid());
        g.fillRoundedRectangle (area, 6.0f);

        const float envNorm = juce::jlimit (0.0f, 1.0f, (envDb + 60.0f) / 60.0f);
        juce::Rectangle<float> envRect = area.withWidth (area.getWidth() * envNorm);
        envRect = envRect.reduced (2.0f, 4.0f);
        g.setColour (accent.withAlpha (0.85f));
        g.fillRoundedRectangle (envRect, 4.0f);

        const float threshNorm = juce::jlimit (0.0f, 1.0f, (threshDb + 60.0f) / 60.0f);
        const float threshX = area.getX() + area.getWidth() * threshNorm;
        g.setColour (gls::ui::Colours::textSecondary());
        g.drawVerticalLine ((int) std::round (threshX), area.getY(), area.getBottom());

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (12.0f));
        juce::String status = "Env " + juce::String ((int) envDb) + " dB / Thresh " + juce::String ((int) threshDb) + " dB";
        g.drawFittedText (status, area.toNearestInt(), juce::Justification::centred, 1);
    }

    void timerCallback() override { repaint(); }
};

EQDynamicTiltProAudioProcessorEditor::EQDynamicTiltProAudioProcessorEditor (EQDynamicTiltProAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("EQ")),
      headerComponent ("EQ.DynamicTiltPro", "Dynamic Tilt Pro")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<TiltVisualComponent> (processorRef, accentColour);
    addAndMakeVisible (*centerVisual);

    auto makeMacro = [this](juce::Slider& s, const juce::String& label) { configureSlider (s, label, true); };
    auto makeMicro = [this](juce::Slider& s, const juce::String& label, bool linear = false) { configureSlider (s, label, false, linear); };

    makeMacro (tiltSlider,   "Tilt");
    makeMacro (rangeSlider,  "Range");
    makeMacro (threshSlider, "Threshold");
    makeMacro (pivotSlider,  "Pivot");

    makeMicro (attackSlider, "Attack");
    makeMicro (releaseSlider,"Release");
    makeMicro (inputTrimSlider, "Input", true);
    makeMicro (mixSlider,       "Mix", true);
    makeMicro (outputTrimSlider,"Output", true);

    detectorModeBox.setLookAndFeel (&lookAndFeel);
    styleBox.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (detectorModeBox);
    addAndMakeVisible (styleBox);
    configureToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, id, slider));
    };

    attachSlider ("tilt",        tiltSlider);
    attachSlider ("range",       rangeSlider);
    attachSlider ("thresh",      threshSlider);
    attachSlider ("pivot_freq",  pivotSlider);
    attachSlider ("attack",      attackSlider);
    attachSlider ("release",     releaseSlider);
    attachSlider ("input_trim",  inputTrimSlider);
    attachSlider ("mix",         mixSlider);
    attachSlider ("output_trim", outputTrimSlider);

    detectorModeBox.addItem ("Peak", 1);
    detectorModeBox.addItem ("RMS", 2);
    styleBox.addItem ("Classic", 1);
    styleBox.addItem ("Wide", 2);
    styleBox.addItem ("Tight", 3);

    comboAttachments.push_back (std::make_unique<ComboAttachment> (state, "detector_mode", detectorModeBox));
    comboAttachments.push_back (std::make_unique<ComboAttachment> (state, "shelf_style", styleBox));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (940, 500);
}

EQDynamicTiltProAudioProcessorEditor::~EQDynamicTiltProAudioProcessorEditor()
{
    detectorModeBox.setLookAndFeel (nullptr);
    styleBox.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EQDynamicTiltProAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& label,
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

    auto labelComp = std::make_unique<juce::Label>();
    labelComp->setText (label, juce::dontSendNotification);
    labelComp->setJustificationType (juce::Justification::centred);
    labelComp->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    labelComp->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*labelComp);
    labeledSliders.push_back ({ &slider, labelComp.get() });
    sliderLabels.push_back (std::move (labelComp));
}

void EQDynamicTiltProAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void EQDynamicTiltProAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        entry.label->setBounds (sliderBounds.withHeight (18).translated (0, -20));
    }
}

void EQDynamicTiltProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void EQDynamicTiltProAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.33f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.28f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 4;
    tiltSlider   .setBounds (left.removeFromTop (macroHeight).reduced (8));
    rangeSlider  .setBounds (left.removeFromTop (macroHeight).reduced (8));
    threshSlider .setBounds (left.removeFromTop (macroHeight).reduced (8));
    pivotSlider  .setBounds (left.removeFromTop (macroHeight).reduced (8));

    attackSlider.setBounds (right.removeFromTop (right.getHeight() / 3).reduced (8));
    releaseSlider.setBounds (right.removeFromTop (right.getHeight() / 2).reduced (8));
    detectorModeBox.setBounds (right.removeFromTop (32).reduced (4));
    styleBox.setBounds (right.removeFromTop (32).reduced (4));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    mixSlider       .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* EQDynamicTiltProAudioProcessor::createEditor()
{
    return new EQDynamicTiltProAudioProcessorEditor (*this);
}

void EQDynamicTiltProAudioProcessor::applyPreset (int index)
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

void EQDynamicTiltProAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    auto ensureVector = [&](auto& vec)
    {
        if ((int) vec.size() < numChannels)
        {
            juce::dsp::ProcessSpec spec { currentSampleRate,
                                          lastBlockSize > 0 ? lastBlockSize : 512u,
                                          1 };
            const int previous = (int) vec.size();
            vec.resize (numChannels);
            for (int ch = previous; ch < numChannels; ++ch)
            {
                vec[ch].prepare (spec);
                vec[ch].reset();
            }
        }
    };

    ensureVector (lowShelves);
    ensureVector (highShelves);
    if ((int) envelopes.size() < numChannels)
        envelopes.resize (numChannels, 0.0f);
}

void EQDynamicTiltProAudioProcessor::updateFilters (float totalTiltDb, float pivotFreq, float shelfQ)
{
    if (currentSampleRate <= 0.0)
        return;

    const float limitedPivot = juce::jlimit (80.0f, (float) (currentSampleRate * 0.45f), pivotFreq);
    const float halfTilt = juce::jlimit (-18.0f, 18.0f, totalTiltDb) * 0.5f;
    const float lowGain  = juce::Decibels::decibelsToGain (-halfTilt);
    const float highGain = juce::Decibels::decibelsToGain (halfTilt);

    auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, limitedPivot, shelfQ, lowGain);
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, limitedPivot, shelfQ, highGain);

    for (auto& filter : lowShelves)
        filter.coefficients = lowCoeffs;
    for (auto& filter : highShelves)
        filter.coefficients = highCoeffs;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQDynamicTiltProAudioProcessor();
}
