#include "GLSSubCommandAudioProcessor.h"

GLSSubCommandAudioProcessor::GLSSubCommandAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUB_COMMAND", createParameterLayout())
{
}

void GLSSubCommandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize();

    originalBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                            (int) lastBlockSize,
                            false, false, true);

    for (auto& state : channelStates)
    {
        state.lowPass.reset();
        state.outputHPF.reset();
        state.envelope = 0.0f;
        state.gain = 1.0f;
    }
}

void GLSSubCommandAudioProcessor::releaseResources()
{
}

void GLSSubCommandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto* bypassParam = apvts.getRawParameterValue ("ui_bypass");
    const bool softBypass = bypassParam != nullptr && bypassParam->load() > 0.5f;
    if (softBypass)
        return;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto xoverFreq = read ("xover_freq");
    const auto subLevelDb= read ("sub_level");
    const auto tightness = juce::jlimit (0.0f, 1.0f, read ("tightness"));
    const auto harmonics = juce::jlimit (0.0f, 1.0f, read ("harmonics"));
    const auto outHpf    = read ("out_hpf");
    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputTrim = juce::Decibels::decibelsToGain (read ("input_trim"));
    const auto outputTrim = juce::Decibels::decibelsToGain (read ("output_trim"));

    buffer.applyGain (inputTrim);
    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureStateSize();

    originalBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    originalBuffer.makeCopyOf (buffer, true);

    const auto subGain     = juce::Decibels::decibelsToGain (subLevelDb);
    const auto attackCoeff = std::exp (-1.0f / ((0.5f + tightness * 4.5f) * 0.001f * currentSampleRate));
    const auto releaseCoeff= std::exp (-1.0f / ((10.0f - tightness * 9.0f) * 0.001f * currentSampleRate));

    const int numSamples   = buffer.getNumSamples();
    const int numChannels  = buffer.getNumChannels();

    juce::AudioBuffer<float> lowBuffer (numChannels, numSamples);
    juce::AudioBuffer<float> highBuffer (numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        updateFilters (state, xoverFreq, outHpf);

        const auto* input = originalBuffer.getReadPointer (ch);
        auto* lowPtr  = lowBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            lowPtr[i] = state.lowPass.processSample (input[i]);

        auto* highPtr = highBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            highPtr[i] = input[i] - lowPtr[i];

        for (int i = 0; i < numSamples; ++i)
        {
            auto& env = state.envelope;
            const auto level = std::abs (lowPtr[i]);
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const auto targetGain = env > 0.0f ? juce::jmap (tightness, 1.0f, 0.5f) : 1.0f;
            state.gain += 0.02f * (targetGain - state.gain);

            float sample = lowPtr[i] * state.gain;
            sample = generateHarmonics (sample, harmonics);
            sample *= subGain;
            lowPtr[i] = sample;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* lowPtr  = lowBuffer.getWritePointer (ch);
        auto* highPtr = highBuffer.getWritePointer (ch);
        auto* outPtr  = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float processed = lowPtr[i] + highPtr[i];
            const float original  = originalBuffer.getSample (ch, i);
            auto sample = processed * mix + original * (1.0f - mix);
            outPtr[i] = state.outputHPF.processSample (sample);
        }
    }

    buffer.applyGain (outputTrim);
}

void GLSSubCommandAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSSubCommandAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSSubCommandAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("xover_freq", "Xover Freq",
                                                                   juce::NormalisableRange<float> (40.0f, 250.0f, 0.01f, 0.4f), 90.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_level",  "Sub Level",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tightness",  "Tightness",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("harmonics",  "Harmonics",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("out_hpf",    "Out HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 120.0f, 0.01f, 0.4f), 35.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim", "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

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

class SubCommandVisual : public juce::Component, private juce::Timer
{
public:
    SubCommandVisual (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
        : apvts (state), accent (accentColour)
    {
        xover     = apvts.getRawParameterValue ("xover_freq");
        subLevel  = apvts.getRawParameterValue ("sub_level");
        tightness = apvts.getRawParameterValue ("tightness");
        harmonics = apvts.getRawParameterValue ("harmonics");
        mix       = apvts.getRawParameterValue ("mix");
        outHpf    = apvts.getRawParameterValue ("out_hpf");
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.5f);

        auto freqNorm = normaliseLog (xover != nullptr ? xover->load() : 90.0f, 40.0f, 250.0f);
        auto hpfNorm  = normaliseLog (outHpf != nullptr ? outHpf->load() : 35.0f, 20.0f, 120.0f);
        auto mixValue = mix != nullptr ? mix->load() : 1.0f;
        auto harmonicValue = harmonics != nullptr ? harmonics->load() : 0.0f;
        auto tightValue = tightness != nullptr ? tightness->load() : 0.5f;
        auto subDb = subLevel != nullptr ? subLevel->load() : 0.0f;

        auto freqArea = bounds.removeFromTop (bounds.getHeight() * 0.6f);
        auto freqLine = freqArea;

        g.setColour (gls::ui::Colours::grid());
        for (int i = 0; i <= 4; ++i)
        {
            const auto x = juce::jmap ((float) i / 4.0f, 0.0f, 1.0f, freqLine.getX(), freqLine.getRight());
            g.drawVerticalLine ((int) x, freqLine.getY(), freqLine.getBottom());
        }

        const auto xoverX = freqLine.getX() + freqLine.getWidth() * freqNorm;
        const auto hpfX   = freqLine.getX() + freqLine.getWidth() * hpfNorm;

        g.setColour (accent);
        g.drawLine (xoverX, freqLine.getY(), xoverX, freqLine.getBottom(), 2.0f);

        g.setColour (accent.withMultipliedAlpha (0.6f));
        g.drawLine (hpfX, freqLine.getY(), hpfX, freqLine.getBottom(), 1.5f);

        g.setFont (gls::ui::makeFont (12.0f, true));
        g.setColour (gls::ui::Colours::text());
        g.drawFittedText ("Xover", juce::Rectangle<int> ((int) xoverX - 40, (int) freqLine.getY() - 18, 80, 16),
                          juce::Justification::centred, 1);
        g.drawFittedText ("Out HPF", juce::Rectangle<int> ((int) hpfX - 45, (int) freqLine.getBottom() + 4, 90, 16),
                          juce::Justification::centred, 1);

        auto bars = bounds.reduced (12.0f);
        auto barHeight = bars.getHeight() / 3.0f;

        auto drawBar = [&g](juce::Rectangle<float> area, float value, const juce::String& label, juce::Colour colour)
        {
            g.setColour (gls::ui::Colours::grid());
            g.drawRect (area);
            auto fill = area.withWidth (area.getWidth() * juce::jlimit (0.0f, 1.0f, value));
            g.setColour (colour);
            g.fillRect (fill);
            g.setColour (gls::ui::Colours::textSecondary());
            g.setFont (gls::ui::makeFont (11.0f));
            g.drawFittedText (label, area.toNearestInt().translated (0, -16), juce::Justification::centredLeft, 1);
        };

        drawBar (bars.removeFromTop (barHeight).reduced (0, 4),
                 juce::jlimit (0.0f, 1.0f, (subDb + 12.0f) / 24.0f),
                 "Sub Level", accent.withMultipliedAlpha (0.8f));
        drawBar (bars.removeFromTop (barHeight).reduced (0, 4),
                 tightValue,
                 "Tightness", accent.withMultipliedAlpha (0.6f));
        drawBar (bars.removeFromTop (barHeight).reduced (0, 4),
                 harmonicValue,
                 "Harmonics", accent.withMultipliedAlpha (0.5f));

        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (11.0f));
        g.drawFittedText ("Dry/Wet " + juce::String (juce::roundToInt (mixValue * 100.0f)) + "%",
                          getLocalBounds().removeFromBottom (18),
                          juce::Justification::centredRight, 1);
    }

    void timerCallback() override { repaint(); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;

    std::atomic<float>* xover = nullptr;
    std::atomic<float>* subLevel = nullptr;
    std::atomic<float>* tightness = nullptr;
    std::atomic<float>* harmonics = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outHpf = nullptr;
};

GLSSubCommandAudioProcessorEditor::GLSSubCommandAudioProcessorEditor (GLSSubCommandAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.SubCommand", "Sub Command")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<SubCommandVisual> (processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (xoverFreqSlider, "Xover Freq", true);
    configureSlider (subLevelSlider,  "Sub Level",  true);
    configureSlider (tightnessSlider, "Tightness",  true);
    configureSlider (harmonicsSlider, "Harmonics",  true);
    configureSlider (outHpfSlider,    "Out HPF",    false);
    configureSlider (inputTrimSlider, "Input",      false, true);
    configureSlider (dryWetSlider,    "Dry / Wet",  false, true);
    configureSlider (outputTrimSlider,"Output",     false, true);

    bypassButton.setLookAndFeel (&lookAndFeel);
    bypassButton.setToggleState (false, juce::dontSendNotification);
    addAndMakeVisible (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attach = [this, &state](const char* paramID, juce::Slider& slider)
    {
        attachments.push_back (std::make_unique<SliderAttachment> (state, paramID, slider));
    };

    attach ("xover_freq",  xoverFreqSlider);
    attach ("sub_level",   subLevelSlider);
    attach ("tightness",   tightnessSlider);
    attach ("harmonics",   harmonicsSlider);
    attach ("out_hpf",     outHpfSlider);
    attach ("mix",         dryWetSlider);
    attach ("input_trim",  inputTrimSlider);
    attach ("output_trim", outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (960, 540);
}

GLSSubCommandAudioProcessorEditor::~GLSSubCommandAudioProcessorEditor()
{
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSSubCommandAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& labelText,
                                                         bool isMacro, bool isLinear)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (isLinear ? juce::Slider::LinearHorizontal
                                    : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 70 : 64, 22);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::thumbColourId, accentColour);
    slider.setColour (juce::Slider::trackColourId, accentColour);
    addAndMakeVisible (slider);

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    labeledSliders.push_back ({ &slider, label.get() });
    sliderLabels.push_back (std::move (label));
}

void GLSSubCommandAudioProcessorEditor::layoutLabels()
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

void GLSSubCommandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().reduced (8);
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.3f));
    g.fillRoundedRectangle (body.toFloat(), 8.0f);
}

void GLSSubCommandAudioProcessorEditor::resized()
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
    xoverFreqSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    subLevelSlider .setBounds (left.removeFromTop (macroHeight).reduced (8));
    tightnessSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    harmonicsSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));

    outHpfSlider.setBounds (right.removeFromTop (right.getHeight() * 0.6f).reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;

    inputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    dryWetSlider   .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8).withHeight (footerArea.getHeight() - 16));

    layoutLabels();
}

juce::AudioProcessorEditor* GLSSubCommandAudioProcessor::createEditor()
{
    return new GLSSubCommandAudioProcessorEditor (*this);
}


void GLSSubCommandAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = juce::jmax (0, getTotalNumOutputChannels());
    if (requiredChannels <= 0)
    {
        channelStates.clear();
        originalBuffer.setSize (0, 0);
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
        state.lowPass.prepare (spec);
        state.outputHPF.prepare (spec);
    }

    originalBuffer.setSize (requiredChannels, (int) (lastBlockSize > 0 ? lastBlockSize : 512u), false, false, true);
}

void GLSSubCommandAudioProcessor::updateFilters (ChannelState& state, float xoverFreq, float outHpfFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (20.0f, (float) (currentSampleRate * 0.45), xoverFreq));
    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                       juce::jlimit (10.0f, (float) (currentSampleRate * 0.45), outHpfFreq));

    state.lowPass.coefficients   = lp;
    state.outputHPF.coefficients = hp;
}

float GLSSubCommandAudioProcessor::generateHarmonics (float sample, float amount)
{
    if (amount <= 0.0f)
        return sample;

    const float second = sample * sample * (sample >= 0.0f ? 1.0f : -1.0f);
    const float saturated = std::tanh (sample * juce::jmap (amount, 1.0f, 5.0f));
    return juce::jmap (amount, sample, 0.5f * (saturated + second));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSSubCommandAudioProcessor();
}
