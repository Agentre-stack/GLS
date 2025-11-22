#include "GLSStemBalancerAudioProcessor.h"

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

GLSStemBalancerAudioProcessor::GLSStemBalancerAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STEM_BALANCER", createParameterLayout())
{
}

void GLSStemBalancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize();
    dryBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                       (int) lastBlockSize, false, false, true);

    for (auto& state : channelStates)
    {
        state.lowShelf.reset();
        state.highShelf.reset();
        state.presenceBell.reset();
        state.lowTightHpf.reset();
    }
}

void GLSStemBalancerAudioProcessor::releaseResources()
{
}

void GLSStemBalancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const bool bypassed   = apvts.getRawParameterValue ("ui_bypass")->load() > 0.5f;
    if (bypassed)
        return;

    const auto stemGainDb = get ("stem_gain");
    const auto tilt       = get ("tilt");
    const auto presence   = get ("presence");
    const auto lowTight   = get ("low_tight");
    const bool autoGain   = apvts.getRawParameterValue ("auto_gain")->load() > 0.5f;
    const auto mix        = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const auto inputTrim  = juce::Decibels::decibelsToGain (get ("input_trim"));
    const auto outputTrim = juce::Decibels::decibelsToGain (get ("output_trim"));

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureStateSize();
    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    updateFilters (tilt, presence, lowTight);
    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);

    const auto stemGain = juce::Decibels::decibelsToGain (stemGainDb);
    double preEnergy = 0.0;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);
        const auto* dryData = dryBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float original = dryData[i];
            preEnergy += original * original;

            float sample = original;
            sample = state.lowShelf.processSample (sample);
            sample = state.highShelf.processSample (sample);
            sample = state.presenceBell.processSample (sample);
            sample = state.lowTightHpf.processSample (sample);
            sample *= stemGain;

            data[i] = sample;
        }
    }

    double postEnergy = 0.0;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            postEnergy += data[i] * data[i];
    }

    if (autoGain && postEnergy > 0.0 && preEnergy > 0.0)
    {
        const auto compensation = std::sqrt (preEnergy / postEnergy);
        buffer.applyGain ((float) compensation);
    }

    if (mix < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* dryData = dryBuffer.getReadPointer (ch);
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
                data[i] = data[i] * mix + dryData[i] * (1.0f - mix);
        }
    }

    buffer.applyGain (outputTrim);
}

void GLSStemBalancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSStemBalancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSStemBalancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("stem_gain", "Stem Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt",      "Tilt",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence",  "Presence",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_tight", "Low Tight",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("auto_gain", "Auto Gain", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",       "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim","Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim","Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

class StemBalancerVisual : public juce::Component, private juce::Timer
{
public:
    StemBalancerVisual (juce::AudioProcessorValueTreeState& stateRef, juce::Colour accentColour)
        : apvts (stateRef), accent (accentColour)
    {
        tilt     = apvts.getRawParameterValue ("tilt");
        presence = apvts.getRawParameterValue ("presence");
        lowTight = apvts.getRawParameterValue ("low_tight");
        stemGain = apvts.getRawParameterValue ("stem_gain");
        mix      = apvts.getRawParameterValue ("mix");
        autoGain = apvts.getRawParameterValue ("auto_gain");
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (6.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.4f);

        auto responseArea = bounds.reduced (14.0f);
        responseArea.removeFromBottom (32.0f);
        drawResponse (g, responseArea);
        drawInfo (g, bounds);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;
    std::atomic<float>* tilt = nullptr;
    std::atomic<float>* presence = nullptr;
    std::atomic<float>* lowTight = nullptr;
    std::atomic<float>* stemGain = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* autoGain = nullptr;

    void timerCallback() override { repaint(); }

    void drawResponse (juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto lowDb = tilt != nullptr ? tilt->load() : 0.0f;
        auto highDb = -lowDb;
        auto presenceDb = presence != nullptr ? presence->load() : 0.0f;

        auto mapDbToY = [area](float db)
        {
            const auto norm = juce::jlimit (-12.0f, 12.0f, db) / 24.0f;
            return area.getCentreY() - norm * area.getHeight() * 0.8f;
        };

        const auto leftX = area.getX();
        const auto rightX = area.getRight();
        const auto midNorm = normaliseLogFreq (2500.0f, 20.0f, 20000.0f);
        const auto midX = area.getX() + area.getWidth() * midNorm;

        juce::Path response;
        response.startNewSubPath (leftX, mapDbToY (lowDb));
        response.quadraticTo (midX, mapDbToY ((lowDb + highDb) * 0.5f + presenceDb),
                              rightX, mapDbToY (highDb));

        g.setColour (accent.withAlpha (0.12f));
        juce::Path fill;
        juce::PathStrokeType (10.0f).createStrokedPath (fill, response);
        g.fillPath (fill);

        g.setColour (accent);
        g.strokePath (response, juce::PathStrokeType (2.0f));

        const auto hpfFreq = juce::jmap (lowTight != nullptr ? lowTight->load() : 0.5f, 20.0f, 160.0f);
        const auto hpfNorm = normaliseLogFreq (hpfFreq, 20.0f, 20000.0f);
        const auto hpfX = area.getX() + area.getWidth() * hpfNorm;
        g.setColour (gls::ui::Colours::grid());
        g.drawVerticalLine ((int) std::round (hpfX), area.getY(), area.getBottom());
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (11.0f));
        g.drawFittedText ("Low Tight", juce::Rectangle<int> ((int) hpfX - 30, (int) area.getY() - 18, 60, 16),
                          juce::Justification::centred, 1);
    }

    void drawInfo (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (12.0f));
        juce::String info;
        info << "Stem Gain "
             << juce::String (stemGain != nullptr ? stemGain->load() : 0.0f, 1) << " dB   ";
        info << "Mix " << juce::String ((mix != nullptr ? mix->load() : 1.0f) * 100.0f, 1) << "%   ";
        info << (autoGain != nullptr && autoGain->load() > 0.5f ? "Auto Gain: ON" : "Auto Gain: OFF");
        g.drawFittedText (info, area.removeFromBottom (32.0f).toNearestInt(),
                          juce::Justification::centred, 1);
    }
};

GLSStemBalancerAudioProcessorEditor::GLSStemBalancerAudioProcessorEditor (GLSStemBalancerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.StemBalancer", "Stem Balancer")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<StemBalancerVisual> (processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (stemGainSlider, "Stem Gain", true);
    configureSlider (tiltSlider,     "Tilt",      true);
    configureSlider (presenceSlider, "Presence",  true);
    configureSlider (lowTightSlider, "Low Tight", true);

    configureSlider (inputTrimSlider, "Input",    false, true);
    configureSlider (mixSlider,       "Stem Mix", false, true);
    configureSlider (outputTrimSlider,"Output",   false, true);

    configureToggle (autoGainButton);
    configureToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, id, slider));
    };

    attachSlider ("stem_gain",  stemGainSlider);
    attachSlider ("tilt",       tiltSlider);
    attachSlider ("presence",   presenceSlider);
    attachSlider ("low_tight",  lowTightSlider);
    attachSlider ("input_trim", inputTrimSlider);
    attachSlider ("mix",        mixSlider);
    attachSlider ("output_trim", outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "auto_gain", autoGainButton));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (900, 520);
}

GLSStemBalancerAudioProcessorEditor::~GLSStemBalancerAudioProcessorEditor()
{
    autoGainButton.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSStemBalancerAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name,
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

void GLSStemBalancerAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GLSStemBalancerAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        entry.label->setBounds (sliderBounds.withHeight (18).translated (0, -20));
    }
}

void GLSStemBalancerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void GLSStemBalancerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.33f)).reduced (12);
    auto right = body.removeFromRight (150).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 4;
    stemGainSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    tiltSlider    .setBounds (left.removeFromTop (macroHeight).reduced (8));
    presenceSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    lowTightSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));

    autoGainButton.setBounds (right.removeFromTop (36).reduced (4));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    mixSlider       .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* GLSStemBalancerAudioProcessor::createEditor()
{
    return new GLSStemBalancerAudioProcessorEditor (*this);
}

void GLSStemBalancerAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = juce::jmax (0, getTotalNumOutputChannels());
    if (requiredChannels <= 0)
    {
        channelStates.clear();
        return;
    }

    channelStates.resize ((size_t) requiredChannels);

    const juce::dsp::ProcessSpec spec {
        currentSampleRate > 0.0 ? currentSampleRate : 44100.0,
        lastBlockSize > 0 ? lastBlockSize : 512u,
        1
    };

    for (auto& state : channelStates)
    {
        state.lowShelf.prepare (spec);
        state.highShelf.prepare (spec);
        state.presenceBell.prepare (spec);
        state.lowTightHpf.prepare (spec);
    }
}

void GLSStemBalancerAudioProcessor::updateFilters (float tilt, float presenceDb, float lowTightAmount)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lowGain  = juce::Decibels::decibelsToGain (tilt);
    const auto highGain = juce::Decibels::decibelsToGain (-tilt);
    const auto presenceGain = juce::Decibels::decibelsToGain (presenceDb);
    const auto hpfFreq = juce::jmap (lowTightAmount, 20.0f, 160.0f);

    for (auto& state : channelStates)
    {
        state.lowShelf.coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 250.0f, 0.707f, lowGain);
        state.highShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 4000.0f, 0.707f, highGain);
        state.presenceBell.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 2500.0f, 0.9f, presenceGain);
        state.lowTightHpf.coefficients  = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpfFreq);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSStemBalancerAudioProcessor();
}
