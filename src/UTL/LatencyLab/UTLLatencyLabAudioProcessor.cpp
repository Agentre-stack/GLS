#include "UTLLatencyLabAudioProcessor.h"

namespace
{
constexpr auto kParamLatency      = "latency_ms";
constexpr auto kParamPingEnable   = "ping_enable";
constexpr auto kParamPingInterval = "ping_interval";
constexpr auto kParamPingLevel    = "ping_level";
constexpr auto kParamMix          = "mix";
constexpr auto kParamInputTrim    = "input_trim";
constexpr auto kParamOutputTrim   = "output_trim";
constexpr auto kParamBypass       = "ui_bypass";

class LatencyVisualComponent : public juce::Component, private juce::Timer
{
public:
    LatencyVisualComponent (UTLLatencyLabAudioProcessor& processorRef, juce::Colour accentColour)
        : processor (processorRef), accent (accentColour)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 12.0f, 1.5f);

        auto axisArea = bounds.reduced (20.0f, 28.0f);
        g.setColour (gls::ui::Colours::grid());
        for (int i = 0; i <= 5; ++i)
        {
            const auto x = axisArea.getX() + axisArea.getWidth() * (float) i / 5.0f;
            g.drawVerticalLine ((int) x, axisArea.getY(), axisArea.getBottom());
        }
        g.drawHorizontalLine ((int) axisArea.getCentreY(), axisArea.getX(), axisArea.getRight());

        const float maxLatencyMs = 500.0f;
        const float maxPingMs = 4000.0f;
        const float latencyMs = processor.getLatencyMs();
        const float pingMs    = processor.getPingIntervalMs();
        const float pingActivity = processor.getPingActivity();

        const float latencyNorm = juce::jlimit (0.0f, 1.0f, latencyMs / maxLatencyMs);
        const float pingNorm    = juce::jlimit (0.0f, 1.0f, pingMs / maxPingMs);

        auto latencyX = axisArea.getX() + axisArea.getWidth() * latencyNorm;
        auto pingX    = axisArea.getX() + axisArea.getWidth() * pingNorm;

        g.setColour (accent.withMultipliedAlpha (0.8f));
        g.drawLine ((float) latencyX, axisArea.getY(), (float) latencyX, axisArea.getBottom(), 3.0f);
        g.setColour (accent.withMultipliedAlpha (0.5f));
        g.drawLine ((float) pingX, axisArea.getY(), (float) pingX, axisArea.getBottom(), 2.0f);

        if (pingActivity > 0.01f)
        {
            g.setColour (accent.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, pingActivity)));
            auto glow = axisArea.withSizeKeepingCentre (axisArea.getWidth() * 0.15f, 30.0f)
                                 .withX ((float) pingX - axisArea.getWidth() * 0.075f);
            g.fillRoundedRectangle (glow, 8.0f);
        }

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        g.drawFittedText ("Latency: " + juce::String (latencyMs, 1) + " ms", bounds.removeFromTop (24).toNearestInt(),
                          juce::Justification::centredLeft, 1);
        g.drawFittedText ("Ping interval: " + juce::String (pingMs, 1) + " ms",
                          bounds.removeFromTop (24).toNearestInt(),
                          juce::Justification::centredLeft, 1);
    }

    void timerCallback() override { repaint(); }

private:
    UTLLatencyLabAudioProcessor& processor;
    juce::Colour accent;
};
} // namespace

UTLLatencyLabAudioProcessor::UTLLatencyLabAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LATENCY_LAB", createParameterLayout())
{
}

void UTLLatencyLabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
    updateLatency();

    const int defaultInterval = (int) std::round (apvts.getRawParameterValue (kParamPingInterval)->load()
                                                  * 0.001f * currentSampleRate);
    pingIntervalSamples = juce::jmax (1, defaultInterval);
    pingCounterSamples  = pingIntervalSamples;
}

void UTLLatencyLabAudioProcessor::releaseResources()
{
}

void UTLLatencyLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    ensureStateSize (numChannels);

    auto read = [this](const char* paramId)
    {
        return apvts.getRawParameterValue (paramId)->load();
    };

    const float latencyMs       = read (kParamLatency);
    const float mix             = juce::jlimit (0.0f, 1.0f, read (kParamMix));
    const float inputTrimGain   = juce::Decibels::decibelsToGain (read (kParamInputTrim));
    const float outputTrimGain  = juce::Decibels::decibelsToGain (read (kParamOutputTrim));
    const bool pingEnabled      = read (kParamPingEnable) > 0.5f;
    const float pingIntervalMs  = read (kParamPingInterval);
    pingEnabledFlag.store (pingEnabled);

    const int latencySamples = (int) std::round (latencyMs * 0.001f * currentSampleRate);
    if (latencySamples != lastLatencySamples)
    {
        lastLatencySamples = latencySamples;
        setLatencySamples (latencySamples);
        for (auto& state : channelDelays)
        {
            state.delay.reset();
            state.delay.setDelay ((float) latencySamples);
        }
    }

    const int desiredPingSamples = juce::jmax (1, (int) std::round (pingIntervalMs * 0.001f * currentSampleRate));
    if (desiredPingSamples != pingIntervalSamples)
    {
        pingIntervalSamples = desiredPingSamples;
        pingCounterSamples = juce::jmin (pingCounterSamples, pingIntervalSamples);
    }

    pingLevelLinear = juce::Decibels::decibelsToGain (read (kParamPingLevel));

    buffer.applyGain (inputTrimGain);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    bool pingTriggered = false;

    if (! pingEnabled)
    {
        pingPulseSamples = 0;
        pingCounterSamples = pingIntervalSamples;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float pingSample = 0.0f;
        if (pingEnabled)
        {
            if (pingCounterSamples <= 0)
            {
                pingCounterSamples = juce::jmax (1, pingIntervalSamples);
                pingPulseSamples = 1;
            }

            if (pingPulseSamples > 0)
            {
                pingSample = pingLevelLinear;
                --pingPulseSamples;
                pingTriggered = true;
            }

            --pingCounterSamples;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& delay = channelDelays[(size_t) ch].delay;
            const float drySample = dryBuffer.getReadPointer (ch)[sample] + pingSample;
            const float delayed = delay.popSample (0);
            delay.pushSample (0, drySample);
            buffer.getWritePointer (ch)[sample] = delayed;
        }
    }

    if (mix < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
        }
    }

    buffer.applyGain (outputTrimGain);

    auto activity = pingActivity.load();
    pingActivity.store (pingTriggered ? 1.0f : activity * 0.92f);
}

void UTLLatencyLabAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void UTLLatencyLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLLatencyLabAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLatency, "Latency (ms)",
                                                                   juce::NormalisableRange<float> (0.0f, 500.0f, 0.01f, 0.45f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamPingEnable, "Ping Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamPingInterval, "Ping Interval",
                                                                   juce::NormalisableRange<float> (100.0f, 4000.0f, 0.01f, 0.45f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamPingLevel, "Ping Level",
                                                                   juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void UTLLatencyLabAudioProcessor::ensureStateSize (int numChannels)
{
    if ((int) channelDelays.size() < numChannels)
        channelDelays.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (delaySpecSampleRate, currentSampleRate)
                             || delaySpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      targetBlock > 0 ? targetBlock : 512u,
                                      1 };
        for (auto& state : channelDelays)
        {
            state.delay.prepare (spec);
            state.delay.reset();
        }
        delaySpecSampleRate = currentSampleRate;
        delaySpecBlockSize  = targetBlock;
    }
}

void UTLLatencyLabAudioProcessor::updateLatency()
{
    const float latencyMs = apvts.getRawParameterValue (kParamLatency)->load();
    lastLatencySamples = (int) std::round (latencyMs * 0.001f * (float) currentSampleRate);
    setLatencySamples (lastLatencySamples);
}

float UTLLatencyLabAudioProcessor::getLatencyMs() const noexcept
{
    return currentSampleRate > 0.0 ? ((float) lastLatencySamples / (float) currentSampleRate) * 1000.0f : 0.0f;
}

float UTLLatencyLabAudioProcessor::getPingIntervalMs() const noexcept
{
    return currentSampleRate > 0.0 ? ((float) pingIntervalSamples / (float) currentSampleRate) * 1000.0f : 0.0f;
}

float UTLLatencyLabAudioProcessor::getPingActivity() const noexcept
{
    return juce::jlimit (0.0f, 1.0f, pingActivity.load());
}

UTLLatencyLabAudioProcessorEditor::UTLLatencyLabAudioProcessorEditor (UTLLatencyLabAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.LatencyLab", "Latency Lab")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    heroVisual = std::make_unique<LatencyVisualComponent> (processorRef, accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);
    addAndMakeVisible (heroVisual.get());

    auto configureMacro = [this](juce::Slider& slider, const juce::String& text)
    {
        configureRotarySlider (slider, text);
    };

    auto configureLinear = [this](juce::Slider& slider, const juce::String& text)
    {
        configureLinearSlider (slider, text);
    };

    configureMacro (latencySlider, "Latency");
    configureMacro (pingIntervalSlider, "Ping Interval");
    configureMacro (pingLevelSlider, "Ping Level");
    configureLinear (mixSlider, "Mix");
    configureLinear (inputTrimSlider, "Input Trim");
    configureLinear (outputTrimSlider, "Output Trim");
    configureToggle (pingEnableButton, "Ping");
    configureToggle (bypassButton, "Soft Bypass");

    auto& state = processorRef.getValueTreeState();
    const std::initializer_list<std::pair<juce::Slider*, const char*>> sliderPairs
    {
        { &latencySlider,      kParamLatency },
        { &pingIntervalSlider, kParamPingInterval },
        { &pingLevelSlider,    kParamPingLevel },
        { &mixSlider,          kParamMix },
        { &inputTrimSlider,    kParamInputTrim },
        { &outputTrimSlider,   kParamOutputTrim }
    };

    for (const auto& pair : sliderPairs)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, pair.second, *pair.first));

    const std::initializer_list<std::pair<juce::ToggleButton*, const char*>> buttonPairs
    {
        { &pingEnableButton, kParamPingEnable },
        { &bypassButton,     kParamBypass }
    };

    for (const auto& pair : buttonPairs)
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, pair.second, *pair.first));

    addAndMakeVisible (latencySlider);
    addAndMakeVisible (pingIntervalSlider);
    addAndMakeVisible (pingLevelSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);
    addAndMakeVisible (pingEnableButton);
    addAndMakeVisible (bypassButton);

    setSize (880, 520);
}

UTLLatencyLabAudioProcessorEditor::~UTLLatencyLabAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void UTLLatencyLabAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
}

void UTLLatencyLabAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerArea = bounds.removeFromTop (72);
    auto footerArea = bounds.removeFromBottom (72);
    headerComponent.setBounds (headerArea);
    footerComponent.setBounds (footerArea);

    auto body = bounds.reduced (16);
    auto macroArea = body.removeFromLeft ((int) (body.getWidth() * 0.32f)).reduced (8);
    auto heroArea  = body.removeFromLeft ((int) (body.getWidth() * 0.40f)).reduced (8);
    auto microArea = body.reduced (8);

    heroVisual->setBounds (heroArea);

    auto layoutColumn = [](juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps)
    {
        const int rowHeight = area.getHeight() / (int) comps.size();
        int y = area.getY();
        for (auto* comp : comps)
        {
            comp->setBounds (area.getX(), y, area.getWidth(), rowHeight);
            y += rowHeight;
        }
    };

    layoutColumn (macroArea,
    {
        &latencySlider, &pingIntervalSlider, &pingLevelSlider
    });

    auto linearArea = microArea.removeFromTop ((int) (microArea.getHeight() * 0.6f));
    layoutColumn (linearArea,
    {
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    });

    auto toggleArea = microArea.reduced (8);
    const int toggleHeight = 34;
    pingEnableButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    bypassButton.setBounds (toggleArea.removeFromTop (toggleHeight));
}

void UTLLatencyLabAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, gls::ui::Colours::outline());

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (13.0f));
    label->attachToComponent (&slider, false);
    labels.push_back (std::move (label));
}

void UTLLatencyLabAudioProcessorEditor::configureLinearSlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::trackColourId, accentColour);

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (12.0f));
    label->attachToComponent (&slider, false);
    labels.push_back (std::move (label));
}

void UTLLatencyLabAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setColour (juce::ToggleButton::tickColourId, accentColour);
}

juce::AudioProcessorEditor* UTLLatencyLabAudioProcessor::createEditor()
{
    return new UTLLatencyLabAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLLatencyLabAudioProcessor();
}
