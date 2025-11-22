#include "UTLSignalTracerAudioProcessor.h"

namespace
{
constexpr auto kParamTapSelect   = "tap_select";
constexpr auto kParamPhaseView   = "phase_view";
constexpr auto kParamPeakHold    = "peak_hold";
constexpr auto kParamRmsWindow   = "rms_window";
constexpr auto kParamRoutingMode = "routing_mode";
constexpr auto kParamInputTrim   = "input_trim";
constexpr auto kParamOutputTrim  = "output_trim";
constexpr auto kParamBypass      = "ui_bypass";
constexpr const char* kTapLabelProps[]  = { "tap_label_0", "tap_label_1", "tap_label_2", "tap_label_3" };
constexpr const char* kTapPresetProps[] = { "tap_preset_0", "tap_preset_1", "tap_preset_2" };
} // namespace

UTLSignalTracerAudioProcessor::UTLSignalTracerAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                                   .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    updateTapLabelsFromState();
}

void UTLSignalTracerAudioProcessor::prepareToPlay (double, int samplesPerBlock)
{
    const auto channels = juce::jmax (1, getTotalNumInputChannels());
    inputSnapshot.setSize (channels, samplesPerBlock, false, false, true);
    sideSnapshot.setSize (2, samplesPerBlock, false, false, true);
    postSnapshot.setSize (juce::jmax (1, getTotalNumOutputChannels()), samplesPerBlock, false, false, true);
    for (auto& metric : tapMetrics)
        metric = {};
    rmsAverages.fill (0.0f);
    peakHoldValues.fill (0.0f);
}

void UTLSignalTracerAudioProcessor::releaseResources()
{
    inputSnapshot.setSize (0, 0);
    sideSnapshot.setSize (0, 0);
    postSnapshot.setSize (0, 0);
}

bool UTLSignalTracerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void UTLSignalTracerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples        = buffer.getNumSamples();
    const auto sampleRate        = juce::jmax (1.0, getSampleRate());

    for (auto ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto tapSelect   = apvts.getRawParameterValue (kParamTapSelect)->load();
    const auto phaseView   = apvts.getRawParameterValue (kParamPhaseView)->load();
    const auto peakHold    = apvts.getRawParameterValue (kParamPeakHold)->load();
    const auto rmsWindowMs = apvts.getRawParameterValue (kParamRmsWindow)->load();
    const auto routingMode = apvts.getRawParameterValue (kParamRoutingMode)->load();
    const auto inputTrim   = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (kParamInputTrim)->load());
    const auto outputTrim  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (kParamOutputTrim)->load());
    const bool bypassed    = apvts.getRawParameterValue (kParamBypass)->load() > 0.5f;

    if (bypassed)
        return;

    buffer.applyGain (inputTrim);

    if (inputSnapshot.getNumChannels() < buffer.getNumChannels()
        || inputSnapshot.getNumSamples() < numSamples)
        inputSnapshot.setSize (buffer.getNumChannels(), numSamples, false, false, true);

    inputSnapshot.makeCopyOf (buffer, true);

    if (sideSnapshot.getNumSamples() < numSamples)
        sideSnapshot.setSize (2, numSamples, false, false, true);

    if (postSnapshot.getNumChannels() < buffer.getNumChannels()
        || postSnapshot.getNumSamples() < numSamples)
        postSnapshot.setSize (buffer.getNumChannels(), numSamples, false, false, true);
    postSnapshot.makeCopyOf (buffer, true);

    if (buffer.getNumChannels() >= 2)
    {
        auto* left  = inputSnapshot.getReadPointer (0);
        auto* right = inputSnapshot.getReadPointer (1);
        auto* sideL = sideSnapshot.getWritePointer (0);
        auto* sideR = sideSnapshot.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float side = 0.5f * (left[i] - right[i]);
            sideL[i] = sideR[i] = side;
        }
    }
    else
    {
        sideSnapshot.copyFrom (0, 0, inputSnapshot, 0, 0, numSamples);
        sideSnapshot.copyFrom (1, 0, inputSnapshot, 0, 0, numSamples);
    }

    const int tapIndex = juce::jlimit (0, 3, static_cast<int> (tapSelect));
    const float windowSeconds = juce::jmax (0.005f, rmsWindowMs) * 0.001f;
    const float smoothingCoeff = std::exp (static_cast<float> (-numSamples)
                                           / (windowSeconds * static_cast<float> (sampleRate)));
    const bool holdPeaks = peakHold > 0.5f;

    if (routingMode == 1 && buffer.getNumChannels() >= 2) // Mid/Side view
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (left[i] + right[i]);
            const float side = 0.5f * (left[i] - right[i]);
            left[i]  = mid;
            right[i] = side;
        }
    }
    else if (routingMode == 2) // Solo tap
    {
        const juce::AudioBuffer<float>* source = &getTapBufferForCopy (tapIndex);
        const auto srcChannels = source->getNumChannels();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const int srcChannel = juce::jlimit (0, srcChannels - 1, ch);
            buffer.copyFrom (ch, 0, *source, srcChannel, 0, numSamples);
        }
    }
    else if (tapIndex == 3)
    {
        if (buffer.getNumChannels() >= 2)
        {
            auto* left  = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                const float side = 0.5f * (left[i] - right[i]);
                left[i] = right[i] = side;
            }
        }
        else
        {
            buffer.makeCopyOf (sideSnapshot, true);
        }
    }

    computeTapMetrics (0, inputSnapshot, smoothingCoeff, holdPeaks);
    computeTapMetrics (1, inputSnapshot, smoothingCoeff, holdPeaks);
    computeTapMetrics (2, buffer, smoothingCoeff, holdPeaks);
    computeTapMetrics (3, sideSnapshot, smoothingCoeff, holdPeaks);
    updatePhaseCorrelation (buffer);

    buffer.applyGain (outputTrim);
}

juce::AudioProcessorEditor* UTLSignalTracerAudioProcessor::createEditor()
{
    return new UTLSignalTracerAudioProcessorEditor (*this);
}

void UTLSignalTracerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void UTLSignalTracerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            updateTapLabelsFromState();
        }
}

juce::AudioProcessorValueTreeState::ParameterLayout UTLSignalTracerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.emplace_back (std::make_unique<juce::AudioParameterChoice> (kParamTapSelect, "Tap Select",
                                                                       juce::StringArray { "Input", "Pre", "Post", "Side" }, 0));
    params.emplace_back (std::make_unique<juce::AudioParameterChoice> (kParamPhaseView, "Phase View",
                                                                       juce::StringArray { "Lissajous", "Correlation", "Vectorscope" }, 0));
    params.emplace_back (std::make_unique<juce::AudioParameterBool> (kParamPeakHold, "Peak Hold", false));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamRmsWindow, "RMS Window (ms)",
                                                                      juce::NormalisableRange<float> (5.0f, 500.0f, 0.1f, 0.4f), 50.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterChoice> (kParamRoutingMode, "Routing Mode",
                                                                       juce::StringArray { "Stereo", "Mid/Side", "Solo Tap" }, 0));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                      juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                      juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterBool> (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

namespace
{
class SignalTracerVisualComponent : public juce::Component, private juce::Timer
{
public:
    SignalTracerVisualComponent (UTLSignalTracerAudioProcessor& processorRef,
                                 juce::AudioProcessorValueTreeState& stateRef,
                                 juce::Colour accentColour)
        : processor (processorRef), state (stateRef), accent (accentColour)
    {
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.5f);

        auto waveformArea = bounds.removeFromTop (bounds.getHeight() * 0.65f).reduced (12.0f);
        drawWaveform (g, waveformArea);

        auto metersArea = bounds.reduced (12.0f);
        drawMeters (g, metersArea);
    }

private:
    UTLSignalTracerAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    juce::Colour accent;
    juce::AudioBuffer<float> snapshot;
    std::array<UTLSignalTracerAudioProcessor::TapMetrics, 4> metrics {};
    float phaseCorr = 0.0f;
    int lastTap = 0;
    int phaseMode = 0;
    juce::String tapName;

    void drawWaveform (juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto labelArea = area.removeFromTop (18.0f);
        juce::String viewName = phaseMode == 0 ? "Lissajous"
                                 : phaseMode == 1 ? "Correlation" : "Vectorscope";
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Tap: " + tapName + "  |  View: " + viewName,
                          labelArea.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setColour (gls::ui::Colours::grid());
        g.drawRect (area);

        if (snapshot.getNumSamples() == 0)
            return;

        juce::Path path;
        const auto* data = snapshot.getReadPointer (0);
        const int samples = snapshot.getNumSamples();
        const float midY = area.getCentreY();
        const float scaleX = area.getWidth() / (float) samples;
        const float scaleY = area.getHeight() * 0.45f;

        path.startNewSubPath (area.getX(), midY);
        for (int i = 0; i < samples; ++i)
        {
            const float x = area.getX() + i * scaleX;
            const float y = midY - data[i] * scaleY;
            path.lineTo (x, y);
        }

        g.setColour (accent);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    void drawMeters (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Tap RMS / Peak", area.removeFromTop (16).toNearestInt(),
                          juce::Justification::centredLeft, 1);

        auto meterBounds = area.removeFromTop (24);
        const auto current = metrics[(size_t) juce::jlimit (0, 3, lastTap)];
        auto rmsWidth = meterBounds.getWidth() * current.rms;
        auto peakWidth = meterBounds.getWidth() * current.peak;
        g.setColour (accent.withAlpha (0.4f));
        g.fillRect (meterBounds.withWidth (peakWidth));
        g.setColour (accent);
        g.fillRect (meterBounds.withWidth (rmsWidth));

        auto correlationArea = area.removeFromTop (32).reduced (0, 8);
        g.setColour (gls::ui::Colours::textSecondary());
        g.drawFittedText ("Correlation", correlationArea.toNearestInt().translated (0, -14),
                          juce::Justification::centredLeft, 1);
        auto corrRect = correlationArea.withHeight (12).reduced (0, 4);
        g.setColour (gls::ui::Colours::grid());
        g.drawRect (corrRect);
        auto corrFill = corrRect.withWidth (corrRect.getWidth() * juce::jmap (phaseCorr, -1.0f, 1.0f, 0.0f, 1.0f));
        g.setColour (accent);
        g.fillRect (corrFill);
    }

    void timerCallback() override
    {
        lastTap = juce::jlimit (0, 3, (int) std::round (state.getRawParameterValue (kParamTapSelect)->load()));
        phaseMode = juce::jlimit (0, 2, (int) std::round (state.getRawParameterValue (kParamPhaseView)->load()));
        tapName = processor.getTapLabel (lastTap);
        processor.copyTapBuffer (lastTap, snapshot);
        processor.copyTapMetrics (metrics);
        phaseCorr = processor.getPhaseCorrelation();
        repaint();
    }
};
} // namespace

//==============================================================================
UTLSignalTracerAudioProcessorEditor::UTLSignalTracerAudioProcessorEditor (UTLSignalTracerAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.SignalTracer", "Signal Tracer")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    visualComponent = std::make_unique<SignalTracerVisualComponent> (processorRef, processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*visualComponent);

    tapBox.setLookAndFeel (&lookAndFeel);
    phaseViewBox.setLookAndFeel (&lookAndFeel);
    routingModeBox.setLookAndFeel (&lookAndFeel);
    presetBox.setLookAndFeel (&lookAndFeel);

    tapBox.addItemList ({ "Input", "Pre", "Post", "Side" }, 1);
    phaseViewBox.addItemList ({ "Lissajous", "Correlation", "Vectorscope" }, 1);
    routingModeBox.addItemList ({ "Stereo", "Mid/Side", "Solo Tap" }, 1);
    presetBox.addItemList ({ "Slot 1", "Slot 2", "Slot 3" }, 1);
    tapBox.setSelectedId (1);
    phaseViewBox.setSelectedId (1);
    routingModeBox.setSelectedId (1);
    presetBox.setSelectedId (1);

    peakHoldButton.setLookAndFeel (&lookAndFeel);
    bypassButton.setLookAndFeel (&lookAndFeel);

    rmsWindowSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    rmsWindowSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 20);
    rmsWindowSlider.setLookAndFeel (&lookAndFeel);

    configureSlider (inputTrimSlider, "Input", true);
    configureSlider (outputTrimSlider, "Output", true);

    tapLabelEditor.setText (processorRef.getTapLabel (0), juce::dontSendNotification);
    tapLabelEditor.setColour (juce::TextEditor::backgroundColourId, gls::ui::Colours::panel());
    tapLabelEditor.setColour (juce::TextEditor::textColourId, gls::ui::Colours::text());
    tapLabelEditor.setBorder (juce::BorderSize<int> (4));

    addAndMakeVisible (tapBox);
    addAndMakeVisible (phaseViewBox);
    addAndMakeVisible (peakHoldButton);
    addAndMakeVisible (routingModeBox);
    addAndMakeVisible (rmsWindowSlider);
    addAndMakeVisible (tapLabelEditor);
    addAndMakeVisible (presetBox);
    addAndMakeVisible (savePresetButton);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);
    addAndMakeVisible (bypassButton);

    auto& vts = processorRef.getValueTreeState();
    tapAttachment      = std::make_unique<ComboAttachment>  (vts, kParamTapSelect,   tapBox);
    phaseAttachment    = std::make_unique<ComboAttachment>  (vts, kParamPhaseView,   phaseViewBox);
    peakHoldAttachment = std::make_unique<ButtonAttachment> (vts, kParamPeakHold,    peakHoldButton);
    rmsAttachment      = std::make_unique<SliderAttachment> (vts, kParamRmsWindow,   rmsWindowSlider);
    routingAttachment  = std::make_unique<ComboAttachment>  (vts, kParamRoutingMode, routingModeBox);
    inputTrimAttachment= std::make_unique<SliderAttachment> (vts, kParamInputTrim,   inputTrimSlider);
    outputTrimAttachment=std::make_unique<SliderAttachment> (vts, kParamOutputTrim,  outputTrimSlider);
    bypassAttachment   = std::make_unique<ButtonAttachment> (vts, kParamBypass,      bypassButton);

    tapBox.onChange = [this]
    {
        const int tap = tapBox.getSelectedId() - 1;
        tapLabelEditor.setText (processorRef.getTapLabel (tap), juce::dontSendNotification);
    };

    tapLabelEditor.onTextChange = [this]
    {
        const int tap = tapBox.getSelectedId() - 1;
        processorRef.setTapLabel (tap, tapLabelEditor.getText());
        refreshTapLabels();
    };

    presetBox.onChange = [this]
    {
        const int slot = presetBox.getSelectedId() - 1;
        if (slot >= 0)
        {
            processorRef.loadTapPreset (slot);
            refreshTapLabels();
            tapLabelEditor.setText (processorRef.getTapLabel (tapBox.getSelectedId() - 1), juce::dontSendNotification);
        }
    };

    savePresetButton.onClick = [this]
    {
        const int slot = presetBox.getSelectedId() - 1;
        if (slot >= 0)
            processorRef.storeTapPreset (slot);
    };

    setSize (940, 520);
    refreshTapLabels();
    tapBox.onChange();
}

UTLSignalTracerAudioProcessorEditor::~UTLSignalTracerAudioProcessorEditor()
{
    tapBox.setLookAndFeel (nullptr);
    phaseViewBox.setLookAndFeel (nullptr);
    routingModeBox.setLookAndFeel (nullptr);
    presetBox.setLookAndFeel (nullptr);
    peakHoldButton.setLookAndFeel (nullptr);
    rmsWindowSlider.setLookAndFeel (nullptr);
    inputTrimSlider.setLookAndFeel (nullptr);
    outputTrimSlider.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void UTLSignalTracerAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& name, bool isMacro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 70 : 60, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    addAndMakeVisible (slider);
}

void UTLSignalTracerAudioProcessorEditor::refreshTapLabels()
{
    for (int i = 0; i < 4; ++i)
        tapBox.changeItemText (i + 1, processorRef.getTapLabel (i));
}

void UTLSignalTracerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.3f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void UTLSignalTracerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.33f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.25f)).reduced (12);
    auto centre = body.reduced (12);

    if (visualComponent != nullptr)
        visualComponent->setBounds (centre);

    auto row = left.removeFromTop (32);
    tapBox.setBounds (row);
    row = left.removeFromTop (28);
    tapLabelEditor.setBounds (row);
    row = left.removeFromTop (28);
    presetBox.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (0, 2));
    savePresetButton.setBounds (row.reduced (0, 2));

    row = left.removeFromTop (32);
    phaseViewBox.setBounds (row);
    row = left.removeFromTop (32);
    routingModeBox.setBounds (row);
    peakHoldButton.setBounds (left.removeFromTop (32));

    rmsWindowSlider.setBounds (right.removeFromTop (right.getHeight() / 2).reduced (4));
    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 3;
    inputTrimSlider .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton    .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLSignalTracerAudioProcessor();
}

void UTLSignalTracerAudioProcessor::computeTapMetrics (int tapIndex,
                                                       const juce::AudioBuffer<float>& source,
                                                       float smoothingCoeff,
                                                       bool holdPeaks)
{
    const int channels = source.getNumChannels();
    const int samples  = source.getNumSamples();
    if (channels == 0 || samples == 0 || tapIndex < 0 || tapIndex >= (int) tapMetrics.size())
        return;

    double sumSquares = 0.0;
    float peak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = source.getReadPointer (ch);
        for (int i = 0; i < samples; ++i)
        {
            const float sample = data[i];
            sumSquares += sample * sample;
            peak = juce::jmax (peak, std::abs (sample));
        }
    }

    const float instantaneousRms = std::sqrt (static_cast<float> (sumSquares / (samples * channels)));
    const float alpha = juce::jlimit (0.0f, 0.9999f, smoothingCoeff);
    rmsAverages[tapIndex] = alpha * rmsAverages[tapIndex] + (1.0f - alpha) * instantaneousRms;

    if (holdPeaks)
        peakHoldValues[tapIndex] = juce::jmax (peakHoldValues[tapIndex] * 0.95f, peak);
    else
        peakHoldValues[tapIndex] = peak;

    const std::scoped_lock lock (metricsMutex);
    tapMetrics[tapIndex].rms  = rmsAverages[tapIndex];
    tapMetrics[tapIndex].peak = peakHoldValues[tapIndex];
}

void UTLSignalTracerAudioProcessor::copyTapMetrics (std::array<TapMetrics, 4>& dest) const
{
    const std::scoped_lock lock (metricsMutex);
    dest = tapMetrics;
}

const juce::AudioBuffer<float>& UTLSignalTracerAudioProcessor::getTapBufferForCopy (int tapIndex) const
{
    const int clamped = juce::jlimit (0, 3, tapIndex);
    if (clamped == 2)
        return postSnapshot;
    if (clamped == 3)
        return sideSnapshot;
    return inputSnapshot;
}

void UTLSignalTracerAudioProcessor::copyTapBuffer (int tapIndex, juce::AudioBuffer<float>& dest) const
{
    const auto& source = getTapBufferForCopy (tapIndex);
    dest.setSize (source.getNumChannels(), source.getNumSamples(), false, false, true);
    dest.makeCopyOf (source, true);
}

juce::String UTLSignalTracerAudioProcessor::getTapLabel (int index) const
{
    const int clamped = juce::jlimit (0, 3, index);
    return tapLabels[clamped];
}

void UTLSignalTracerAudioProcessor::setTapLabel (int index, const juce::String& text)
{
    const int clamped = juce::jlimit (0, 3, index);
    tapLabels[clamped] = text;
    if (clamped < 4)
        apvts.state.setProperty (kTapLabelProps[clamped], text, nullptr);
}

void UTLSignalTracerAudioProcessor::storeTapPreset (int slot)
{
    if (slot < 0 || slot >= (int) tapLabelPresets.size())
        return;

    tapLabelPresets[slot] = tapLabels;
    juce::StringArray tokens;
    for (const auto& label : tapLabels)
        tokens.add (label);
    apvts.state.setProperty (kTapPresetProps[slot], tokens.joinIntoString ("|"), nullptr);
}

void UTLSignalTracerAudioProcessor::loadTapPreset (int slot)
{
    if (slot < 0 || slot >= (int) tapLabelPresets.size())
        return;

    auto prop = apvts.state.getProperty (kTapPresetProps[slot]);
    if (prop.isString())
    {
        auto tokens = juce::StringArray::fromTokens (prop.toString(), "|", "");
        if (tokens.size() == (int) tapLabels.size())
        {
            for (int i = 0; i < tokens.size(); ++i)
                tapLabels[(size_t) i] = tokens[i];
        }
    }
    else
    {
        tapLabels = tapLabelPresets[slot];
    }

    for (int i = 0; i < (int) tapLabels.size(); ++i)
        apvts.state.setProperty (kTapLabelProps[i], tapLabels[(size_t) i], nullptr);
}

void UTLSignalTracerAudioProcessor::updateTapLabelsFromState()
{
    for (int i = 0; i < 4; ++i)
    {
        if (apvts.state.hasProperty (kTapLabelProps[i]))
            tapLabels[(size_t) i] = apvts.state.getProperty (kTapLabelProps[i]).toString();
        else
            apvts.state.setProperty (kTapLabelProps[i], tapLabels[(size_t) i], nullptr);
    }

    for (int slot = 0; slot < (int) tapLabelPresets.size(); ++slot)
    {
        if (apvts.state.hasProperty (kTapPresetProps[slot]))
        {
            auto tokens = juce::StringArray::fromTokens (apvts.state.getProperty (kTapPresetProps[slot]).toString(), "|", "");
            if (tokens.size() == (int) tapLabelPresets[slot].size())
            {
                for (int i = 0; i < tokens.size(); ++i)
                    tapLabelPresets[slot][(size_t) i] = tokens[i];
            }
        }
    }
}

void UTLSignalTracerAudioProcessor::updatePhaseCorrelation (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
    {
        phaseCorrelation.store (0.0f);
        return;
    }

    const auto* left = buffer.getReadPointer (0);
    const auto* right= buffer.getReadPointer (1);
    double sumLR = 0.0;
    double sumL2 = 0.0;
    double sumR2 = 0.0;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double l = left[i];
        const double r = right[i];
        sumLR += l * r;
        sumL2 += l * l;
        sumR2 += r * r;
    }

    const double denom = std::sqrt (sumL2 * sumR2) + 1.0e-9;
    phaseCorrelation.store ((float) juce::jlimit (-1.0, 1.0, sumLR / denom));
}
