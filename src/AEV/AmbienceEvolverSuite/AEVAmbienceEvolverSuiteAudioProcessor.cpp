#include "AEVAmbienceEvolverSuiteAudioProcessor.h"

AEVAmbienceEvolverSuiteAudioProcessor::AEVAmbienceEvolverSuiteAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AMBIENCE_EVOLVER", createParameterLayout())
{
}

void AEVAmbienceEvolverSuiteAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
    dryBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                       (int) lastBlockSize, false, false, true);
    for (auto& state : channelStates)
        state = {};
}

void AEVAmbienceEvolverSuiteAudioProcessor::releaseResources()
{
}

void AEVAmbienceEvolverSuiteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const auto ambienceLevel = juce::jlimit (0.0f, 1.0f, get ("ambience_level"));
    const auto deVerb        = juce::jlimit (0.0f, 1.0f, get ("deverb"));
    const auto noiseSupp     = juce::jlimit (0.0f, 1.0f, get ("noise_suppression"));
    const auto transientProt = juce::jlimit (0.0f, 1.0f, get ("transient_protect"));
    const auto toneMatch     = juce::jlimit (0.0f, 1.0f, get ("tone_match"));
    const auto hfRecover     = juce::jlimit (0.0f, 1.0f, get ("hf_recover"));
    const auto outputTrimDb  = get ("output_trim");
    const auto mix           = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const auto inputTrim     = juce::Decibels::decibelsToGain (get ("input_trim"));
    const int profileSlot    = juce::jlimit (0, 2, (int) std::round (apvts.getRawParameterValue ("profile_slot")->load()));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);

    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);

    const float ambienceBlend = ambienceLevel * 0.8f;
    const float deVerbDecay = juce::jmap (deVerb, 0.1f, 0.9f);
    const float toneBlend = toneMatch * 0.5f;
    const float hfGain = juce::Decibels::decibelsToGain (hfRecover * 6.0f);
    const float outputGain = juce::Decibels::decibelsToGain (outputTrimDb);
    const float transientThresh = juce::Decibels::decibelsToGain (-20.0f + transientProt * 10.0f);

    refreshCapturedNoiseSnapshot();
    const float noiseSnapshot = capturedNoiseValue.load();
    double rmsAccumulator = 0.0;
    auto updateRms = [numChannels, numSamples, this](double sumSquares)
    {
        const double denom = juce::jmax (1, numChannels * numSamples);
        const double rms = std::sqrt (sumSquares / denom);
        lastRmsDb.store (juce::Decibels::gainToDecibels ((float) rms));
    };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelStates[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            const float absSample = std::abs (sample);

            state.noiseEstimate = 0.999f * state.noiseEstimate + 0.001f * absSample;
            updateProfileState (absSample, ch);

            const float ambience = state.ambienceState = 0.995f * state.ambienceState + 0.005f * sample;
            const float ambienceRemoved = sample - ambience * ambienceBlend;

            const float smear = state.toneState = state.toneState + deVerbDecay * (ambienceRemoved - state.toneState);
            float cleaned = ambienceRemoved - smear * deVerb;

            const float noiseFloor = juce::jmax (state.noiseEstimate, noiseSnapshot + 1.0e-6f);
            const float gate = juce::jlimit (0.0f, 1.0f, (absSample - noiseFloor) / (noiseFloor + 1.0e-6f));
            const float noiseReduction = juce::jmap (noiseSupp, 0.0f, 1.0f, gate, gate * 0.2f);
            cleaned *= noiseReduction;

            const float transientEnv = state.transientState = juce::jmax (absSample, state.transientState * 0.97f);
            if (transientEnv > transientThresh)
                cleaned = juce::jlimit (-std::abs (sample), std::abs (sample),
                                        cleaned + (sample - cleaned) * transientProt * 0.6f);

            const float toneTarget = sample * 0.5f;
            cleaned = cleaned * (1.0f - toneBlend) + toneTarget * toneBlend;

            const float hfState = state.toneState = 0.98f * state.toneState + 0.02f * cleaned;
            const float hfSignal = cleaned - hfState;
            cleaned += hfSignal * (hfGain - 1.0f);

            data[i] = cleaned;
            rmsAccumulator += (double) cleaned * (double) cleaned;
        }
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

    buffer.applyGain (outputGain);
    updateRms (rmsAccumulator);
}

void AEVAmbienceEvolverSuiteAudioProcessor::triggerProfileCapture()
{
    profileCaptureArmed = true;
    profileSamplesRemaining = (int) currentSampleRate / 2;
    profileAccumulators.assign (juce::jmax (1, getTotalNumInputChannels()), 0.0f);
    profileTotalSamples = profileSamplesRemaining;
    profileProgress.store (0.0f);
}

void AEVAmbienceEvolverSuiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void AEVAmbienceEvolverSuiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
AEVAmbienceEvolverSuiteAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ambience_level", "Ambience Level",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deverb",          "De-Verb",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("noise_suppression","Noise",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("transient_protect","Transient",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone_match",      "Tone Match",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hf_recover",      "HF Recover",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",     "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",             "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim",      "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("ui_bypass",       "Soft Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("profile_slot",   "Profile Slot",
                                                                    juce::StringArray { "Slot 1", "Slot 2", "Slot 3" }, 0));

    return { params.begin(), params.end() };
}

struct AmbienceVisualComponent : public juce::Component, private juce::Timer
{
    AmbienceVisualComponent (AEVAmbienceEvolverSuiteAudioProcessor& proc, juce::Colour accentColour)
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

        auto rmsMeter = bounds.removeFromRight (64.0f).reduced (10.0f);
        drawRmsMeter (g, rmsMeter);

        auto infoArea = bounds.reduced (12.0f);
        drawCaptureStatus (g, infoArea);
    }

private:
    AEVAmbienceEvolverSuiteAudioProcessor& processor;
    juce::Colour accent;

    void drawRmsMeter (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (area, 6.0f, 1.2f);

        const float rmsDb = processor.getLastRmsDb();
        const float norm = juce::jlimit (0.0f, 1.0f, (rmsDb + 60.0f) / 60.0f);
        auto fill = area.withHeight (area.getHeight() * norm).withY (area.getBottom() - area.getHeight() * norm);
        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (fill, 6.0f);

        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        juce::String label = rmsDb <= -120.0f ? "-inf dB" : juce::String (juce::roundToInt (rmsDb)) + " dB";
        g.drawFittedText ("RMS " + label, area.toNearestInt().translated (0, -18), juce::Justification::centred, 1);
    }

    void drawCaptureStatus (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto progress = processor.getProfileProgress();
        const auto captureLevel = processor.getCapturedNoiseLevel();
        const bool capturing = processor.isProfileCaptureActive();
        const int slot = juce::jlimit (0, 2, (int) std::round (processor.getValueTreeState().getRawParameterValue ("profile_slot")->load())) + 1;

        auto bar = area.removeFromBottom (26.0f);
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (bar, 6.0f, 1.2f);
        auto fill = bar.reduced (4.0f);
        fill.setWidth (fill.getWidth() * progress);
        g.setColour (accent.withAlpha (capturing ? 0.9f : 0.4f));
        g.fillRoundedRectangle (fill, 4.0f);

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f, true));
        juce::String status = capturing ? "Capturing room tone…" : "Profile ready";
        status << " (Slot " << slot << ")";
        g.drawFittedText (status,
                          bar.toNearestInt(), juce::Justification::centred, 1);

        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        juce::String info;
        info << "Noise Ref: "
             << juce::Decibels::gainToDecibels (captureLevel + 1.0e-6f, -100.0f) << " dB | "
             << "Progress " << juce::roundToInt (progress * 100.0f) << "%";
        g.drawFittedText (info, area.toNearestInt(), juce::Justification::centred, 2);
    }

    void timerCallback() override { repaint(); }
};

AEVAmbienceEvolverSuiteAudioProcessorEditor::AEVAmbienceEvolverSuiteAudioProcessorEditor (AEVAmbienceEvolverSuiteAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("AEV")),
      headerComponent ("AEV.AmbienceEvolverSuite", "Ambience Evolver Suite")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<AmbienceVisualComponent> (processorRef, accentColour);
    addAndMakeVisible (*centerVisual);

    auto makeMacro = [this](juce::Slider& s, const juce::String& label) { configureSlider (s, label, true); };
    auto makeMicro = [this](juce::Slider& s, const juce::String& label, bool linear = false) { configureSlider (s, label, false, linear); };

    makeMacro (ambienceSlider,  "Ambience");
    makeMacro (deVerbSlider,    "De-Verb");
    makeMacro (noiseSlider,     "Noise");
    makeMacro (transientSlider, "Transient");
    makeMicro (toneMatchSlider, "Tone Match");
    makeMicro (hfRecoverSlider, "HF Recover");
    makeMicro (mixSlider,       "Ambience Mix", true);
    makeMicro (inputTrimSlider, "Input", true);
    makeMicro (outputTrimSlider,"Output", true);

    configureToggle (bypassButton);
    profileButton.addListener (this);
    profileButton.setLookAndFeel (&lookAndFeel);
    profileButton.setColour (juce::TextButton::buttonColourId, accentColour.withAlpha (0.25f));
    addAndMakeVisible (profileButton);
    profileSlotBox.setLookAndFeel (&lookAndFeel);
    profileSlotBox.addItemList ({ "Slot 1", "Slot 2", "Slot 3" }, 1);
    addAndMakeVisible (profileSlotBox);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, id, slider));
    };

    attachSlider ("ambience_level", ambienceSlider);
    attachSlider ("deverb",         deVerbSlider);
    attachSlider ("noise_suppression", noiseSlider);
    attachSlider ("transient_protect", transientSlider);
    attachSlider ("tone_match",     toneMatchSlider);
    attachSlider ("hf_recover",     hfRecoverSlider);
    attachSlider ("mix",            mixSlider);
    attachSlider ("input_trim",     inputTrimSlider);
    attachSlider ("output_trim",    outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));
    profileSlotAttachment = std::make_unique<ComboAttachment> (state, "profile_slot", profileSlotBox);

    setSize (960, 520);
}

AEVAmbienceEvolverSuiteAudioProcessorEditor::~AEVAmbienceEvolverSuiteAudioProcessorEditor()
{
    profileButton.removeListener (this);
    profileButton.setLookAndFeel (nullptr);
    profileSlotBox.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& label,
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

void AEVAmbienceEvolverSuiteAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &profileButton)
        processorRef.triggerProfileCapture();
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        entry.label->setBounds (sliderBounds.withHeight (18).translated (0, -20));
    }
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.35f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.30f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 4;
    ambienceSlider .setBounds (left.removeFromTop (macroHeight).reduced (8));
    deVerbSlider   .setBounds (left.removeFromTop (macroHeight).reduced (8));
    noiseSlider    .setBounds (left.removeFromTop (macroHeight).reduced (8));
    transientSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));

    toneMatchSlider.setBounds (right.removeFromTop (right.getHeight() / 3).reduced (8));
    hfRecoverSlider.setBounds (right.removeFromTop (right.getHeight() / 2).reduced (8));
    profileSlotBox.setBounds (right.removeFromTop (28).reduced (4));
    profileButton.setBounds (right.removeFromTop (36).reduced (4));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    mixSlider       .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* AEVAmbienceEvolverSuiteAudioProcessor::createEditor()
{
    return new AEVAmbienceEvolverSuiteAudioProcessorEditor (*this);
}

void AEVAmbienceEvolverSuiteAudioProcessor::ensureStateSize (int numChannels)
{
    const auto required = juce::jmax (0, numChannels);
    if (required <= 0)
    {
        channelStates.clear();
        return;
    }

    const auto previous = channelStates.size();
    channelStates.resize ((size_t) required);
    for (size_t i = previous; i < channelStates.size(); ++i)
        channelStates[i] = {};
}

void AEVAmbienceEvolverSuiteAudioProcessor::updateProfileState (float sampleEnv, int channel)
{
    if (! profileCaptureArmed)
        return;

    if (channel < (int) profileAccumulators.size())
        profileAccumulators[(size_t) channel] += sampleEnv;

    const float remaining = (float) juce::jmax (0, profileSamplesRemaining);
    const float progress = 1.0f - (remaining / (float) juce::jmax (1, profileTotalSamples));
    profileProgress.store (juce::jlimit (0.0f, 1.0f, progress));

    if (--profileSamplesRemaining <= 0)
    {
        profileCaptureArmed = false;
        const int slot = juce::jlimit (0, 2, (int) std::round (apvts.getRawParameterValue ("profile_slot")->load()));
        if ((int) capturedProfiles[slot].size() < (int) profileAccumulators.size())
            capturedProfiles[slot].resize (profileAccumulators.size(), 0.0f);

        for (size_t i = 0; i < profileAccumulators.size(); ++i)
        {
            const float averaged = profileAccumulators[i] / (float) juce::jmax (1, profileTotalSamples);
            capturedProfiles[slot][i] = averaged;
        }
        refreshCapturedNoiseSnapshot();
        profileProgress.store (1.0f);
    }
}

void AEVAmbienceEvolverSuiteAudioProcessor::refreshCapturedNoiseSnapshot()
{
    const int slot = juce::jlimit (0, 2, (int) std::round (apvts.getRawParameterValue ("profile_slot")->load()));
    const auto& profile = capturedProfiles[slot];
    float average = 1.0e-6f;
    if (! profile.empty())
    {
        float sum = 0.0f;
        for (auto value : profile)
            sum += juce::jmax (value, 1.0e-6f);
        average = sum / (float) profile.size();
    }
    capturedNoiseValue.store (average);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AEVAmbienceEvolverSuiteAudioProcessor();
}
