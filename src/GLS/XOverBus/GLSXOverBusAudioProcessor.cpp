#include "GLSXOverBusAudioProcessor.h"

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

GLSXOverBusAudioProcessor::GLSXOverBusAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XOVER_BUS", createParameterLayout())
{
}

void GLSXOverBusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    const auto channels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, channels };

    auto prepareBand = [this, &spec](BandFilters& band, int order)
    {
        prepareFilters (band, order, spec);
    };

    const int order = juce::roundToInt (apvts.getRawParameterValue ("slope")->load());
    prepareBand (lowBand, order);
    prepareBand (midBandLow, order);
    prepareBand (midBandHigh, order);
    prepareBand (highBand, order);

    ensureBufferSize ((int) channels, (int) lastBlockSize);
}

void GLSXOverBusAudioProcessor::releaseResources()
{
}

void GLSXOverBusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto split1 = get ("split_freq1");
    const auto split2 = get ("split_freq2");
    const auto slope  = juce::roundToInt (get ("slope"));
    const bool solo1  = apvts.getRawParameterValue ("band_solo1")->load() > 0.5f;
    const bool solo2  = apvts.getRawParameterValue ("band_solo2")->load() > 0.5f;
    const bool solo3  = apvts.getRawParameterValue ("band_solo3")->load() > 0.5f;
    const auto output = get ("output_trim");
    const auto mixAmount = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const auto inputTrim = juce::Decibels::decibelsToGain (get ("input_trim"));

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    const auto numChannels = (juce::uint32) juce::jmax (1, buffer.getNumChannels());

    juce::dsp::ProcessSpec blockSpec { currentSampleRate, lastBlockSize, numChannels };
    prepareFilters (lowBand, slope, blockSpec);
    prepareFilters (midBandLow, slope, blockSpec);
    prepareFilters (midBandHigh, slope, blockSpec);
    prepareFilters (highBand, slope, blockSpec);

    updateCoefficients (lowBand, split1, true);
    updateCoefficients (midBandLow, split1, false);
    updateCoefficients (midBandHigh, split2, true);
    updateCoefficients (highBand, split2, false);

    ensureBufferSize (buffer.getNumChannels(), buffer.getNumSamples());

    buffer.applyGain (inputTrim);
    originalBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    originalBuffer.makeCopyOf (buffer, true);

    lowBuffer.makeCopyOf (buffer);
    midBuffer.makeCopyOf (buffer);
    highBuffer.makeCopyOf (buffer);

    applyFilters (lowBand, lowBuffer, true);
    applyFilters (midBandLow, midBuffer, false);
    applyFilters (midBandHigh, midBuffer, true);
    applyFilters (highBand, highBuffer, false);

    const bool anySolo = solo1 || solo2 || solo3;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* low = lowBuffer.getWritePointer (ch);
        auto* mid = midBuffer.getWritePointer (ch);
        auto* high = highBuffer.getWritePointer (ch);
        auto* out = buffer.getWritePointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sample = 0.0f;
            if (!anySolo || solo1) sample += low[i];
            if (!anySolo || solo2) sample += mid[i];
            if (!anySolo || solo3) sample += high[i];
            const auto original = originalBuffer.getSample (ch, i);
            out[i] = sample * mixAmount + original * (1.0f - mixAmount);
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (output));
}

void GLSXOverBusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSXOverBusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSXOverBusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto freqRange = juce::NormalisableRange<float> (50.0f, 8000.0f, 0.01f, 0.4f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("split_freq1", "Split Freq 1", freqRange, 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("split_freq2", "Split Freq 2", freqRange, 2000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("slope",      "Slope",
                                                                   juce::NormalisableRange<float> (6.0f, 48.0f, 6.0f), 24.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("band_solo1", "Band 1 Solo", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("band_solo2", "Band 2 Solo", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("band_solo3", "Band 3 Solo", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Dry/Wet",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim", "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

class XOverVisual : public juce::Component, private juce::Timer
{
public:
    XOverVisual (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
        : apvts (state), accent (accentColour)
    {
        split1 = apvts.getRawParameterValue ("split_freq1");
        split2 = apvts.getRawParameterValue ("split_freq2");
        slope  = apvts.getRawParameterValue ("slope");
        solo1  = apvts.getRawParameterValue ("band_solo1");
        solo2  = apvts.getRawParameterValue ("band_solo2");
        solo3  = apvts.getRawParameterValue ("band_solo3");
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 10.0f, 1.5f);

        auto freqArea = bounds.reduced (12.0f);
        g.setColour (gls::ui::Colours::grid());
        for (int i = 1; i < 4; ++i)
        {
            const auto x = freqArea.getX() + freqArea.getWidth() * (float) i / 4.0f;
            g.drawLine (x, freqArea.getY(), x, freqArea.getBottom(), 1.0f);
        }

        auto drawSplit = [&g, freqArea, this](std::atomic<float>* param, juce::Colour colour)
        {
            if (param == nullptr)
                return;
            auto freq = param->load();
            auto norm = normaliseLog (freq, 50.0f, 8000.0f);
            auto x = freqArea.getX() + freqArea.getWidth() * norm;
            g.setColour (colour);
            g.drawLine (x, freqArea.getY(), x, freqArea.getBottom(), 2.0f);
        };

        drawSplit (split1, accent);
        drawSplit (split2, accent.withMultipliedAlpha (0.7f));

        auto slopeValue = slope != nullptr ? slope->load() : 24.0f;
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Slope " + juce::String ((int) slopeValue) + " dB",
                          freqArea.toNearestInt().removeFromBottom (20), juce::Justification::centred, 1);

        auto soloArea = bounds.removeFromBottom (32.0f);
        auto drawSolo = [&g, soloArea](int index, std::atomic<float>* param)
        {
            auto segment = soloArea.withWidth (soloArea.getWidth() / 3.0f)
                                   .withX (soloArea.getX() + (float) index * soloArea.getWidth() / 3.0f);
            auto active = param != nullptr && param->load() > 0.5f;
            g.setColour (active ? gls::ui::Colours::text() : gls::ui::Colours::textSecondary());
            g.drawFittedText (active ? "Solo" : "Band", segment.toNearestInt(), juce::Justification::centred, 1);
        };

        drawSolo (0, solo1);
        drawSolo (1, solo2);
        drawSolo (2, solo3);
    }

    void timerCallback() override { repaint(); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;
    std::atomic<float>* split1 = nullptr;
    std::atomic<float>* split2 = nullptr;
    std::atomic<float>* slope  = nullptr;
    std::atomic<float>* solo1  = nullptr;
    std::atomic<float>* solo2  = nullptr;
    std::atomic<float>* solo3  = nullptr;
};

GLSXOverBusAudioProcessorEditor::GLSXOverBusAudioProcessorEditor (GLSXOverBusAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      accentColour (gls::ui::accentForFamily ("GLS")),
      headerComponent ("GLS.XOverBus", "XOver Bus")
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<XOverVisual> (processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (split1Slider, "Split 1", true);
    configureSlider (split2Slider, "Split 2", true);
    configureSlider (slopeSlider,  "Slope",  true);
    configureSlider (inputTrimSlider, "Input", false, true);
    configureSlider (dryWetSlider,   "Dry / Wet", false, true);
    configureSlider (outputTrimSlider, "Output", false, true);

    configureToggle (band1SoloButton, "Low");
    configureToggle (band2SoloButton, "Mid");
    configureToggle (band3SoloButton, "High");
    configureToggle (bypassButton, "Soft Bypass");

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* paramID, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramID, slider));
    };
    auto attachToggle = [this, &state](const char* paramID, juce::ToggleButton& toggle)
    {
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, paramID, toggle));
    };

    attachSlider ("split_freq1", split1Slider);
    attachSlider ("split_freq2", split2Slider);
    attachSlider ("slope",       slopeSlider);
    attachSlider ("input_trim",  inputTrimSlider);
    attachSlider ("mix",         dryWetSlider);
    attachSlider ("output_trim", outputTrimSlider);

    attachToggle ("band_solo1", band1SoloButton);
    attachToggle ("band_solo2", band2SoloButton);
    attachToggle ("band_solo3", band3SoloButton);
    attachToggle ("ui_bypass",  bypassButton);

    setSize (960, 520);
}

GLSXOverBusAudioProcessorEditor::~GLSXOverBusAudioProcessorEditor()
{
    bypassButton.setLookAndFeel (nullptr);
    band1SoloButton.setLookAndFeel (nullptr);
    band2SoloButton.setLookAndFeel (nullptr);
    band3SoloButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GLSXOverBusAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& labelText,
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

void GLSXOverBusAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GLSXOverBusAudioProcessorEditor::layoutLabels()
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

void GLSXOverBusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().reduced (8);
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.3f));
    g.fillRoundedRectangle (body.toFloat(), 8.0f);
}

void GLSXOverBusAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.35f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.25f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 3;
    split1Slider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    split2Slider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    slopeSlider .setBounds (left.removeFromTop (macroHeight).reduced (8));

    auto toggleHeight = right.getHeight() / 3;
    band1SoloButton.setBounds (right.removeFromTop (toggleHeight).reduced (8));
    band2SoloButton.setBounds (right.removeFromTop (toggleHeight).reduced (8));
    band3SoloButton.setBounds (right.removeFromTop (toggleHeight).reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 4;

    inputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    dryWetSlider   .setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    outputTrimSlider.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8));
    bypassButton.setBounds (footerArea.removeFromLeft (slotWidth).reduced (8).withHeight (footerArea.getHeight() - 16));

    layoutLabels();
}

juce::AudioProcessorEditor* GLSXOverBusAudioProcessor::createEditor()
{
    return new GLSXOverBusAudioProcessorEditor (*this);
}


void GLSXOverBusAudioProcessor::prepareFilters (BandFilters& filters, int order, const juce::dsp::ProcessSpec& spec)
{
    const int stages = juce::jmax (1, order / 6);
    if ((int) filters.lowFilters.size() != stages)
    {
        filters.lowFilters.resize (stages);
        for (auto& filter : filters.lowFilters)
            filter.prepare (spec);
    }
    if ((int) filters.highFilters.size() != stages)
    {
        filters.highFilters.resize (stages);
        for (auto& filter : filters.highFilters)
            filter.prepare (spec);
    }
}

void GLSXOverBusAudioProcessor::updateCoefficients (BandFilters& filters, float freq, bool isLow)
{
    if (currentSampleRate <= 0.0)
        return;

    for (auto& filter : filters.lowFilters)
    {
        filter.setType (isLow ? juce::dsp::LinkwitzRileyFilterType::lowpass
                              : juce::dsp::LinkwitzRileyFilterType::highpass);
        filter.setCutoffFrequency (freq);
    }

    for (auto& filter : filters.highFilters)
    {
        filter.setType (isLow ? juce::dsp::LinkwitzRileyFilterType::highpass
                              : juce::dsp::LinkwitzRileyFilterType::lowpass);
        filter.setCutoffFrequency (freq);
    }
}

void GLSXOverBusAudioProcessor::applyFilters (BandFilters& filters, juce::AudioBuffer<float>& buffer, bool useLowSet)
{
    juce::dsp::AudioBlock<float> block (buffer);
    auto& list = useLowSet ? filters.lowFilters : filters.highFilters;
    for (auto& filter : list)
        filter.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void GLSXOverBusAudioProcessor::ensureBufferSize (int channels, int samples)
{
    lowBuffer.setSize (channels, samples, false, false, true);
    midBuffer.setSize (channels, samples, false, false, true);
    highBuffer.setSize (channels, samples, false, false, true);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSXOverBusAudioProcessor();
}
