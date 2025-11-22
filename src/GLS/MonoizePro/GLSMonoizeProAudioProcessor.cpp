#include "GLSMonoizeProAudioProcessor.h"

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

GLSMonoizeProAudioProcessor::GLSMonoizeProAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONOIZE_PRO", createParameterLayout())
{
}

void GLSMonoizeProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec {
        currentSampleRate,
        lastBlockSize,
        1
    };

    monoLowFilter.prepare (spec);
    stereoHighFilter.prepare (spec);
    monoLowFilter.reset();
    stereoHighFilter.reset();

    updateFilters (apvts.getRawParameterValue ("mono_below")->load(),
                   apvts.getRawParameterValue ("stereo_above")->load());
}

void GLSMonoizeProAudioProcessor::releaseResources()
{
}

void GLSMonoizeProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const bool bypassed = apvts.getRawParameterValue ("ui_bypass")->load() > 0.5f;
    if (bypassed)
        return;

    const auto monoBelow   = read ("mono_below");
    const auto stereoAbove = read ("stereo_above");
    const auto widthParam  = juce::jlimit (0.0f, 2.0f, read ("width"));
    const auto centerLift  = juce::Decibels::decibelsToGain (read ("center_lift"));
    const auto sideTrim    = juce::Decibels::decibelsToGain (read ("side_trim"));
    const auto mix         = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputTrim   = juce::Decibels::decibelsToGain (read ("input_trim"));
    const auto outputTrim  = juce::Decibels::decibelsToGain (read ("output_trim"));

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    updateFilters (monoBelow, stereoAbove);

    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float mid  = 0.5f * (left[i] + right[i]);
        float side = 0.5f * (left[i] - right[i]);

        float lowSide = monoLowFilter.processSample (side);
        float highSide = stereoHighFilter.processSample (side);
        float bandSide = side - lowSide - highSide;

        lowSide = 0.0f;
        highSide *= 1.0f + juce::jlimit (0.0f, 1.0f, (stereoAbove - 1000.0f) / 11000.0f);

        side = lowSide + bandSide + highSide;

        mid *= centerLift;
        side *= sideTrim * widthParam;

        left[i]  = mid + side;
        right[i] = mid - side;
    }
    if (mix < 1.0f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
        }
    }

    buffer.applyGain (outputTrim);
}

void GLSMonoizeProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSMonoizeProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSMonoizeProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mono_below",   "Mono Below",
                                                                   juce::NormalisableRange<float> (40.0f, 400.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("stereo_above", "Stereo Above",
                                                                   juce::NormalisableRange<float> (1000.0f, 12000.0f, 0.01f, 0.35f), 3000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",        "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("center_lift",  "Center Lift",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("side_trim",    "Side Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Dry/Wet",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim",   "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",  "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

class MonoizeVisual : public juce::Component, private juce::Timer
{
public:
    MonoizeVisual (juce::AudioProcessorValueTreeState& stateRef, juce::Colour accentColour)
        : state (stateRef), accent (accentColour)
    {
        monoBelow   = state.getRawParameterValue ("mono_below");
        stereoAbove = state.getRawParameterValue ("stereo_above");
        width       = state.getRawParameterValue ("width");
        centerLift  = state.getRawParameterValue ("center_lift");
        sideTrim    = state.getRawParameterValue ("side_trim");
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (6.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.5f);

        auto freqArea = bounds.removeFromTop (bounds.getHeight() * 0.5f).reduced (10.0f);
        const auto monoFreqNorm   = normaliseLog (monoBelow != nullptr ? monoBelow->load() : 120.0f, 40.0f, 400.0f);
        const auto stereoFreqNorm = normaliseLog (stereoAbove != nullptr ? stereoAbove->load() : 3000.0f, 1000.0f, 12000.0f);
        auto monoX   = freqArea.getX() + freqArea.getWidth() * monoFreqNorm;
        auto stereoX = freqArea.getX() + freqArea.getWidth() * stereoFreqNorm;
        g.setColour (gls::ui::Colours::grid());
        g.drawRect (freqArea);
        g.setColour (accent.withAlpha (0.8f));
        g.drawLine (monoX, freqArea.getY(), monoX, freqArea.getBottom(), 2.0f);
        g.setColour (accent.withAlpha (0.6f));
        g.drawLine (stereoX, freqArea.getY(), stereoX, freqArea.getBottom(), 2.0f);
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (11.0f));
        g.drawFittedText ("Mono", juce::Rectangle<int> ((int) monoX - 30, (int) freqArea.getBottom(), 60, 16), juce::Justification::centred, 1);
        g.drawFittedText ("Stereo", juce::Rectangle<int> ((int) stereoX - 30, (int) freqArea.getBottom(), 60, 16), juce::Justification::centred, 1);

        auto barArea = bounds.reduced (18.0f);
        auto midRect  = barArea.removeFromLeft (barArea.getWidth() * 0.5f - 8.0f);
        auto sideRect = barArea.translated (barArea.getWidth() * 0.5f + 8.0f, 0);
        drawMeter (g, midRect, accent, centerLift != nullptr ? centerLift->load() : 0.0f, -12.0f, 12.0f, "Mid");
        drawMeter (g, sideRect, accent.withMultipliedAlpha (0.7f),
                   sideTrim != nullptr ? sideTrim->load() + juce::Decibels::gainToDecibels (width != nullptr ? width->load() : 1.0f) : 0.0f,
                   -12.0f, 12.0f, "Side");
    }

    void timerCallback() override { repaint(); }

private:
    static void drawMeter (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour,
                           float valueDb, float minDb, float maxDb, const juce::String& label)
    {
        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (bounds, 8.0f, 1.2f);
        const auto norm = juce::jlimit (0.0f, 1.0f, (valueDb - minDb) / (maxDb - minDb));
        auto fill = bounds.withHeight (bounds.getHeight() * norm).withY (bounds.getBottom() - bounds.getHeight() * norm);
        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (fill, 8.0f);
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (11.0f));
        g.drawFittedText (label, bounds.toNearestInt().translated (0, -18), juce::Justification::centred, 1);
    }

    juce::AudioProcessorValueTreeState& state;
    juce::Colour accent;
    std::atomic<float>* monoBelow = nullptr;
    std::atomic<float>* stereoAbove = nullptr;
    std::atomic<float>* width = nullptr;
    std::atomic<float>* centerLift = nullptr;
    std::atomic<float>* sideTrim = nullptr;
};

GLSMonoizeProAudioProcessorEditor::GLSMonoizeProAudioProcessorEditor (GLSMonoizeProAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.MonoizePro", "Monoize Pro")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<MonoizeVisual> (processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (monoBelowSlider,   "Mono Below", true);
    configureSlider (stereoAboveSlider, "Stereo Above", true);
    configureSlider (widthSlider,       "Width", true);
    configureSlider (centerLiftSlider,  "Center Lift", false);
    configureSlider (sideTrimSlider,    "Side Trim", false);
    configureSlider (inputTrimSlider,   "Input", false, true);
    configureSlider (dryWetSlider,      "Dry / Wet", false, true);
    configureSlider (outputTrimSlider,  "Output", false, true);
    configureToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* paramId, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramId, slider));
    };

    attachSlider ("mono_below",   monoBelowSlider);
    attachSlider ("stereo_above", stereoAboveSlider);
    attachSlider ("width",        widthSlider);
    attachSlider ("center_lift",  centerLiftSlider);
    attachSlider ("side_trim",    sideTrimSlider);
    attachSlider ("input_trim",   inputTrimSlider);
    attachSlider ("mix",          dryWetSlider);
    attachSlider ("output_trim",  outputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (920, 500);
}

GLSMonoizeProAudioProcessorEditor::~GLSMonoizeProAudioProcessorEditor()
{
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSMonoizeProAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& targetName,
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
    label->setText (targetName, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    labeledSliders.push_back ({ &slider, label.get() });
    sliderLabels.push_back (std::move (label));
}

void GLSMonoizeProAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GLSMonoizeProAudioProcessorEditor::layoutLabels()
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

void GLSMonoizeProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void GLSMonoizeProAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.4f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 3;
    monoBelowSlider  .setBounds (left.removeFromTop (macroHeight).reduced (8));
    stereoAboveSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    widthSlider      .setBounds (left.removeFromTop (macroHeight).reduced (8));

    auto right = centre.removeFromRight (juce::roundToInt (centre.getWidth() * 0.35f)).reduced (12);
    auto rightHeight = right.getHeight() / 2;
    centerLiftSlider.setBounds (right.removeFromTop (rightHeight).reduced (8));
    sideTrimSlider  .setBounds (right.removeFromTop (rightHeight).reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    dryWetSlider    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));

    layoutLabels();
}

juce::AudioProcessorEditor* GLSMonoizeProAudioProcessor::createEditor()
{
    return new GLSMonoizeProAudioProcessorEditor (*this);
}


void GLSMonoizeProAudioProcessor::updateFilters (float monoFreq, float stereoFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (20.0f, (float) (currentSampleRate * 0.45), monoFreq));
    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                       juce::jlimit (100.0f, (float) (currentSampleRate * 0.45), stereoFreq));

    monoLowFilter.coefficients = lp;
    stereoHighFilter.coefficients = hp;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSMonoizeProAudioProcessor();
}
