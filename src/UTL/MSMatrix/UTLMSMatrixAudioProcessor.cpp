#include "UTLMSMatrixAudioProcessor.h"

namespace
{
constexpr auto kParamMidGain     = "mid_gain";
constexpr auto kParamSideGain    = "side_gain";
constexpr auto kParamWidth       = "width_pct";
constexpr auto kParamMonoFold    = "mono_fold";
constexpr auto kParamSideHpf     = "side_hpf";
constexpr auto kParamSideLpf     = "side_lpf";
constexpr auto kParamPhaseMid    = "phase_mid";
constexpr auto kParamPhaseSide   = "phase_side";
constexpr auto kParamMix         = "mix";
constexpr auto kParamInputTrim   = "input_trim";
constexpr auto kParamOutputTrim  = "output_trim";
constexpr auto kParamBypass      = "ui_bypass";

class WidthVisualizer : public juce::Component, private juce::Timer
{
public:
    WidthVisualizer (UTLMSMatrixAudioProcessor& proc, juce::Colour accentColour)
        : processor (proc), accent (accentColour)
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

        auto meterArea = bounds.removeFromTop (bounds.getHeight() * 0.6f).reduced (16.0f);

        auto drawBar = [&](juce::Rectangle<float> area, float value, const juce::String& name)
        {
            g.setColour (gls::ui::Colours::outline());
            g.drawRoundedRectangle (area, 6.0f, 1.4f);

            auto fill = area.withWidth (area.getWidth() * juce::jlimit (0.0f, 1.0f, value));
            g.setColour (accent.withMultipliedAlpha (0.85f));
            g.fillRoundedRectangle (fill, 6.0f);

            g.setColour (gls::ui::Colours::textSecondary());
            g.setFont (gls::ui::makeFont (13.0f));
            g.drawFittedText (name, area.toNearestInt(), juce::Justification::centredLeft, 1);
        };

        auto midBar = meterArea.removeFromTop (meterArea.getHeight() * 0.45f).reduced (4.0f);
        auto sideBar= meterArea.removeFromTop (meterArea.getHeight() * 0.9f).reduced (4.0f);
        drawBar (midBar, processor.getMidMeter(), "Mid");
        drawBar (sideBar, processor.getSideMeter(), "Side");

        auto widthArea = bounds.reduced (20.0f);
        const float widthValue = juce::jlimit (0.0f, 1.0f, processor.getWidthMeter());

        g.setColour (gls::ui::Colours::grid());
        g.drawEllipse (widthArea, 1.2f);
        auto needleLength = widthArea.getWidth() * 0.5f;
        auto angle = juce::MathConstants<float>::pi * (0.5f + widthValue);
        juce::Line<float> needle (widthArea.getCentre(),
                                  widthArea.getCentre() + juce::Point<float> (std::cos (angle), std::sin (angle)) * needleLength);
        g.setColour (accent);
        g.drawLine (needle, 3.0f);

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        g.drawFittedText ("Stereo width", widthArea.toNearestInt().translated (0, (int) widthArea.getHeight() / 2),
                          juce::Justification::centred, 1);
    }

    void timerCallback() override { repaint(); }

private:
    UTLMSMatrixAudioProcessor& processor;
    juce::Colour accent;
};
} // namespace

UTLMSMatrixAudioProcessor::UTLMSMatrixAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MS_MATRIX", createParameterLayout())
{
}

void UTLMSMatrixAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  (juce::uint32) juce::jmax (1, samplesPerBlock),
                                  1 };
    sideHighPass.reset();
    sideLowPass.reset();
    sideHighPass.prepare (spec);
    sideLowPass.prepare (spec);
    updateFilters (apvts.getRawParameterValue (kParamSideHpf)->load(),
                   apvts.getRawParameterValue (kParamSideLpf)->load());
}

void UTLMSMatrixAudioProcessor::releaseResources()
{
}

void UTLMSMatrixAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float midGain   = juce::Decibels::decibelsToGain (read (kParamMidGain));
    const float sideGain  = juce::Decibels::decibelsToGain (read (kParamSideGain));
    const float width     = juce::jlimit (0.0f, 200.0f, read (kParamWidth)) * 0.01f;
    const float monoFold  = juce::jlimit (0.0f, 1.0f, read (kParamMonoFold));
    const bool phaseMid   = read (kParamPhaseMid)  > 0.5f;
    const bool phaseSide  = read (kParamPhaseSide) > 0.5f;
    const float mix       = juce::jlimit (0.0f, 1.0f, read (kParamMix));
    const float inputTrim = juce::Decibels::decibelsToGain (read (kParamInputTrim));
    const float outputTrim= juce::Decibels::decibelsToGain (read (kParamOutputTrim));

    updateFilters (read (kParamSideHpf), read (kParamSideLpf));

    buffer.applyGain (inputTrim);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    auto* left  = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    float midPeak = 0.0f;
    float sidePeak = 0.0f;
    float widthPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right != nullptr ? right[i] : inL;

        float mid = 0.5f * (inL + inR);
        float side = 0.5f * (inL - inR);

        if (phaseMid)
            mid = -mid;
        if (phaseSide)
            side = -side;

        side = sideHighPass.processSample (side);
        side = sideLowPass.processSample (side);

        const float processedMid  = mid * midGain;
        const float processedSide = side * sideGain * width;

        const float stereoLeft  = processedMid + processedSide;
        const float stereoRight = processedMid - processedSide;
        const float monoValue   = processedMid;

        const float outL = juce::jmap (monoFold, stereoLeft, monoValue);
        const float outR = juce::jmap (monoFold, stereoRight, monoValue);

        left[i] = outL;
        if (right != nullptr)
            right[i] = outR;

        midPeak  = juce::jmax (midPeak, std::abs (processedMid));
        sidePeak = juce::jmax (sidePeak, std::abs (processedSide));
        widthPeak= juce::jmax (widthPeak, juce::jlimit (0.0f, 1.0f,
                                                        std::abs (processedSide) / (std::abs (processedMid) + 0.001f)));
    }

    auto smooth = [](std::atomic<float>& target, float value)
    {
        const float blended = target.load() * 0.85f + value * 0.15f;
        target.store (juce::jlimit (0.0f, 1.0f, blended));
    };

    smooth (midMeter,  juce::jlimit (0.0f, 1.0f, midPeak));
    smooth (sideMeter, juce::jlimit (0.0f, 1.0f, sidePeak));
    smooth (widthMeter, widthPeak);

    if (mix < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            for (int sample = 0; sample < numSamples; ++sample)
                wet[sample] = wet[sample] * mix + dry[sample] * (1.0f - mix);
        }
    }

    buffer.applyGain (outputTrim);
}

float UTLMSMatrixAudioProcessor::getMidMeter() const noexcept  { return midMeter.load(); }
float UTLMSMatrixAudioProcessor::getSideMeter() const noexcept { return sideMeter.load(); }
float UTLMSMatrixAudioProcessor::getWidthMeter() const noexcept{ return widthMeter.load(); }

void UTLMSMatrixAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void UTLMSMatrixAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLMSMatrixAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMidGain, "Mid Gain",
                                                                   juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamSideGain, "Side Gain",
                                                                   juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamWidth, "Width %",
                                                                   juce::NormalisableRange<float> (0.0f, 200.0f, 0.01f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMonoFold, "Mono Fold",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamSideHpf, "Side HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 800.0f, 0.01f, 0.4f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamSideLpf, "Side LPF",
                                                                   juce::NormalisableRange<float> (2000.0f, 20000.0f, 0.01f, 0.4f), 12000.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamPhaseMid,  "Phase Mid",  false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamPhaseSide, "Phase Side", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void UTLMSMatrixAudioProcessor::updateFilters (float hpf, float lpf)
{
    if (currentSampleRate <= 0.0)
        return;

    const float safeHpf = juce::jlimit (20.0f, 800.0f, hpf);
    const float safeLpf = juce::jlimit (1000.0f, (float) (currentSampleRate * 0.45f), lpf);
    if (juce::approximatelyEqual (safeHpf, cachedHpf)
        && juce::approximatelyEqual (safeLpf, cachedLpf))
        return;

    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, safeHpf, 0.707f);
    auto lpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate, safeLpf, 0.707f);
    sideHighPass.coefficients = hpfCoeffs;
    sideLowPass.coefficients  = lpfCoeffs;
    cachedHpf = safeHpf;
    cachedLpf = safeLpf;
}

UTLMSMatrixAudioProcessorEditor::UTLMSMatrixAudioProcessorEditor (UTLMSMatrixAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.MSMatrix", "MS Matrix")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    heroVisual = std::make_unique<WidthVisualizer> (processorRef, accentColour);
    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);
    addAndMakeVisible (heroVisual.get());

    auto makeRotary = [this](juce::Slider& slider, const juce::String& label)
    {
        configureRotarySlider (slider, label);
    };
    auto makeLinear = [this](juce::Slider& slider, const juce::String& label, bool horizontal)
    {
        configureLinearSlider (slider, label, horizontal);
    };

    makeRotary (midGainSlider,  "Mid Gain");
    makeRotary (sideGainSlider, "Side Gain");
    makeRotary (widthSlider,    "Width %");
    makeRotary (monoFoldSlider, "Mono Fold");
    makeRotary (sideHpfSlider,  "Side HPF");
    makeRotary (sideLpfSlider,  "Side LPF");
    makeLinear (mixSlider,      "Mix", true);
    makeLinear (inputTrimSlider,"Input Trim", true);
    makeLinear (outputTrimSlider,"Output Trim", true);

    configureToggle (phaseMidButton,  "Phase Mid");
    configureToggle (phaseSideButton, "Phase Side");
    configureToggle (bypassButton,    "Soft Bypass");

    auto& state = processorRef.getValueTreeState();
    const std::initializer_list<std::pair<juce::Slider*, const char*>> sliderPairs =
    {
        { &midGainSlider,   kParamMidGain },
        { &sideGainSlider,  kParamSideGain },
        { &widthSlider,     kParamWidth },
        { &monoFoldSlider,  kParamMonoFold },
        { &sideHpfSlider,   kParamSideHpf },
        { &sideLpfSlider,   kParamSideLpf },
        { &mixSlider,       kParamMix },
        { &inputTrimSlider, kParamInputTrim },
        { &outputTrimSlider,kParamOutputTrim }
    };

    for (const auto& pair : sliderPairs)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, pair.second, *pair.first));

    const std::initializer_list<std::pair<juce::ToggleButton*, const char*>> togglePairs =
    {
        { &phaseMidButton,  kParamPhaseMid },
        { &phaseSideButton, kParamPhaseSide },
        { &bypassButton,    kParamBypass }
    };

    for (const auto& pair : togglePairs)
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, pair.second, *pair.first));

    addAndMakeVisible (midGainSlider);
    addAndMakeVisible (sideGainSlider);
    addAndMakeVisible (widthSlider);
    addAndMakeVisible (monoFoldSlider);
    addAndMakeVisible (sideHpfSlider);
    addAndMakeVisible (sideLpfSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);
    addAndMakeVisible (phaseMidButton);
    addAndMakeVisible (phaseSideButton);
    addAndMakeVisible (bypassButton);

    setSize (860, 520);
}

UTLMSMatrixAudioProcessorEditor::~UTLMSMatrixAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void UTLMSMatrixAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
}

void UTLMSMatrixAudioProcessorEditor::resized()
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
        &midGainSlider, &sideGainSlider, &widthSlider, &monoFoldSlider
    });

    auto filterArea = microArea.removeFromTop ((int) (microArea.getHeight() * 0.45f));
    layoutColumn (filterArea,
    {
        &sideHpfSlider, &sideLpfSlider
    });

    auto linearArea = microArea.removeFromTop ((int) (microArea.getHeight() * 0.5f));
    layoutColumn (linearArea,
    {
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    });

    auto toggleArea = microArea.reduced (8);
    const int toggleHeight = 34;
    phaseMidButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    phaseSideButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    bypassButton.setBounds (toggleArea.removeFromTop (toggleHeight));
}

void UTLMSMatrixAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (13.0f));
    label->attachToComponent (&slider, false);
    labels.push_back (std::move (label));
}

void UTLMSMatrixAudioProcessorEditor::configureLinearSlider (juce::Slider& slider, const juce::String& labelText, bool horizontal)
{
    slider.setSliderStyle (horizontal ? juce::Slider::LinearHorizontal
                                      : juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::trackColourId, accentColour);
    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (12.0f));
    label->attachToComponent (&slider, false);
    labels.push_back (std::move (label));
}

void UTLMSMatrixAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setColour (juce::ToggleButton::tickColourId, accentColour);
}

juce::AudioProcessorEditor* UTLMSMatrixAudioProcessor::createEditor()
{
    return new UTLMSMatrixAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLMSMatrixAudioProcessor();
}
