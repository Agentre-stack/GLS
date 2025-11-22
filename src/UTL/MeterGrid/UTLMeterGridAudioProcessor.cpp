#include "UTLMeterGridAudioProcessor.h"

namespace
{
constexpr auto kParamIntegration = "integration_ms";
constexpr auto kParamPeakHold    = "peak_hold_ms";
constexpr auto kParamScalePreset = "scale_preset";
constexpr auto kParamInputTrim   = "input_trim";
constexpr auto kParamOutputTrim  = "output_trim";
constexpr auto kParamFreeze      = "freeze";
constexpr auto kParamBypass      = "ui_bypass";

class MeterGridVisualComponent : public juce::Component, private juce::Timer
{
public:
    MeterGridVisualComponent (UTLMeterGridAudioProcessor& processorRef, juce::Colour accentColour)
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

        auto snapshot = processor.getMeterSnapshot();
        const float ceiling = processor.getDisplayCeilingDb();
        const float floorDb = ceiling > 0.0f ? -ceiling : -60.0f;

        auto normalise = [floorDb](float db)
        {
            const float clamped = juce::jlimit (floorDb, 0.0f, db);
            return juce::jlimit (0.0f, 1.0f, (clamped - floorDb) / (0.0f - floorDb));
        };

        auto meterArea = bounds.reduced (24.0f);
        meterArea.setHeight (meterArea.getHeight() - 40.0f);

        const juce::String labels[] { "RMS L", "RMS R", "Peak L", "Peak R" };
        const float values[] { snapshot.rmsLeft, snapshot.rmsRight, snapshot.peakLeft, snapshot.peakRight };
        const float holds[]  { snapshot.holdLeft, snapshot.holdRight, snapshot.holdLeft, snapshot.holdRight };

        const int numMeters = 4;
        const float gap = 12.0f;
        const float barWidth = (meterArea.getWidth() - (gap * (numMeters - 1))) / (float) numMeters;

        for (int i = 0; i < numMeters; ++i)
        {
            auto bar = juce::Rectangle<float> (meterArea.getX() + i * (barWidth + gap),
                                               meterArea.getY(),
                                               barWidth,
                                               meterArea.getHeight());

            g.setColour (gls::ui::Colours::grid());
            g.drawRoundedRectangle (bar, 6.0f, 1.4f);

            auto fill = bar;
            fill.removeFromTop (fill.getHeight() * (1.0f - normalise (values[i])));
            g.setColour (accent.withMultipliedAlpha (0.85f));
            g.fillRoundedRectangle (fill, 6.0f);

            const auto holdY = bar.getBottom() - bar.getHeight() * normalise (holds[i]);
            g.setColour (gls::ui::Colours::textSecondary());
            g.drawLine (bar.getX(), holdY, bar.getRight(), holdY, 1.5f);

            g.setColour (gls::ui::Colours::text());
            g.setFont (gls::ui::makeFont (12.0f));
            g.drawFittedText (labels[i], bar.toNearestInt().translated (0, (int) bar.getHeight() + 4),
                              juce::Justification::centred, 1);
        }

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        g.drawFittedText ("Crest: " + juce::String (snapshot.crest, 1) + " dB",
                          bounds.removeFromBottom (24).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    void timerCallback() override { repaint(); }

private:
    UTLMeterGridAudioProcessor& processor;
    juce::Colour accent;
};
} // namespace

namespace
{
constexpr auto kStateId = "METER_GRID";
}

const std::array<UTLMeterGridAudioProcessor::Preset, 3> UTLMeterGridAudioProcessor::presetBank {{
    { "K-20 Broad", {
        { "integration_ms", 300.0f },
        { "peak_hold_ms",   1000.0f },
        { "display_ceiling", -1.0f },
        { "scale_preset", 0.0f },
        { "input_trim",   0.0f },
        { "output_trim",  0.0f },
        { "ui_bypass",    0.0f },
        { "freeze",       0.0f }
    }},
    { "K-14 Modern", {
        { "integration_ms", 400.0f },
        { "peak_hold_ms",   750.0f },
        { "display_ceiling", -0.5f },
        { "scale_preset", 1.0f },
        { "input_trim",   0.0f },
        { "output_trim",  0.0f },
        { "ui_bypass",    0.0f },
        { "freeze",       0.0f }
    }},
    { "Broadcast", {
        { "integration_ms", 600.0f },
        { "peak_hold_ms",   1200.0f },
        { "display_ceiling", -2.0f },
        { "scale_preset", 2.0f },
        { "input_trim",   0.0f },
        { "output_trim", -1.0f },
        { "ui_bypass",    0.0f },
        { "freeze",       0.0f }
    }}
}};

UTLMeterGridAudioProcessor::UTLMeterGridAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
    for (auto& value : meterValues)
        value.store (-60.0f);
    updateScalePreset (0);
}

void UTLMeterGridAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void UTLMeterGridAudioProcessor::releaseResources()
{
}

void UTLMeterGridAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const bool bypassed = apvts.getRawParameterValue (kParamBypass)->load() > 0.5f;
    if (bypassed)
        return;

    auto read = [this](const char* paramId)
    {
        return apvts.getRawParameterValue (paramId)->load();
    };

    const float integrationMs = juce::jlimit (5.0f, 2000.0f, read (kParamIntegration));
    const float peakHoldMs    = juce::jlimit (20.0f, 4000.0f, read (kParamPeakHold));
    const int scaleChoice     = (int) std::round (apvts.getRawParameterValue (kParamScalePreset)->load());
    const bool freezeMeters   = read (kParamFreeze) > 0.5f;
    const float inputGain     = juce::Decibels::decibelsToGain (read (kParamInputTrim));
    const float outputGain    = juce::Decibels::decibelsToGain (read (kParamOutputTrim));

    updateScalePreset (scaleChoice);

    const float integrationCoeff = std::exp (-1.0f / (integrationMs * 0.001f * currentSampleRate));
    const int holdSamples = juce::jmax (1, (int) std::round (peakHoldMs * 0.001f * currentSampleRate));

    buffer.applyGain (inputGain);

    std::array<float, 2> peakInstant { 0.0f, 0.0f };

    for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& rms = rmsState[(size_t) ch];
        auto& hold = peakHoldValue[(size_t) ch];
        auto& holdCounter = peakHoldCountdown[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            const float absSample = std::abs (sample);
            rms = integrationCoeff * rms + (1.0f - integrationCoeff) * (sample * sample);

            if (absSample >= hold)
            {
                hold = absSample;
                holdCounter = holdSamples;
            }
            else if (holdCounter > 0)
            {
                --holdCounter;
            }
            else
            {
                hold *= 0.995f;
            }

            peakInstant[(size_t) ch] = juce::jmax (peakInstant[(size_t) ch], absSample);
        }
    }

    buffer.applyGain (outputGain);

    auto storeMeter = [this, freezeMeters](int index, float value)
    {
        if (! freezeMeters)
            meterValues[(size_t) index].store (value);
    };

    const float rmsLeft = juce::Decibels::gainToDecibels (std::sqrt (rmsState[0]) + 1.0e-6f);
    const float rmsRight= juce::Decibels::gainToDecibels (std::sqrt (rmsState[juce::jmin (1, (int) rmsState.size() - 1)]) + 1.0e-6f);
    const float peakLeft= juce::Decibels::gainToDecibels (peakInstant[0] + 1.0e-6f);
    const float peakRight= juce::Decibels::gainToDecibels (peakInstant[juce::jmin (1, (int) peakInstant.size() - 1)] + 1.0e-6f);
    const float holdLeft= juce::Decibels::gainToDecibels (peakHoldValue[0] + 1.0e-6f);
    const float holdRight= juce::Decibels::gainToDecibels (peakHoldValue[juce::jmin (1, (int) peakHoldValue.size() - 1)] + 1.0e-6f);

    storeMeter (0, rmsLeft);
    storeMeter (1, rmsRight);
    storeMeter (2, peakLeft);
    storeMeter (3, peakRight);
    storeMeter (4, holdLeft);
    storeMeter (5, holdRight);

    const float avgRms = (std::sqrt (rmsState[0]) + std::sqrt (rmsState[juce::jmin (1, (int) rmsState.size() - 1)])) * 0.5f;
    const float crest = juce::Decibels::gainToDecibels ((juce::jmax (peakInstant[0], peakInstant[juce::jmin (1, (int) peakInstant.size() - 1)]) + 1.0e-6f)
                                                        / (avgRms + 1.0e-6f));
    if (! freezeMeters)
        crestValue.store (crest);
}

void UTLMeterGridAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void UTLMeterGridAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLMeterGridAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamIntegration, "Integration",
                                                                   juce::NormalisableRange<float> (10.0f, 1000.0f, 0.01f, 0.4f), 300.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamPeakHold, "Peak Hold",
                                                                   juce::NormalisableRange<float> (50.0f, 4000.0f, 0.01f, 0.4f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (kParamScalePreset, "Scale",
                                                                    juce::StringArray { "Full Scale", "K-12", "K-14", "K-20" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamFreeze, "Freeze", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

UTLMeterGridAudioProcessor::MeterSnapshot UTLMeterGridAudioProcessor::getMeterSnapshot() const noexcept
{
    MeterSnapshot snapshot;
    snapshot.rmsLeft  = meterValues[0].load();
    snapshot.rmsRight = meterValues[1].load();
    snapshot.peakLeft = meterValues[2].load();
    snapshot.peakRight= meterValues[3].load();
    snapshot.holdLeft = meterValues[4].load();
    snapshot.holdRight= meterValues[5].load();
    snapshot.crest    = crestValue.load();
    return snapshot;
}

float UTLMeterGridAudioProcessor::getDisplayCeilingDb() const noexcept
{
    return displayCeiling.load();
}

int UTLMeterGridAudioProcessor::getScalePresetIndex() const noexcept
{
    return scalePreset.load();
}

void UTLMeterGridAudioProcessor::updateScalePreset (int presetIndex)
{
    scalePreset.store (presetIndex);
    switch (presetIndex)
    {
        case 1: displayCeiling.store (12.0f); break;
        case 2: displayCeiling.store (14.0f); break;
        case 3: displayCeiling.store (20.0f); break;
        default: displayCeiling.store (0.0f); break;
    }
}

UTLMeterGridAudioProcessorEditor::UTLMeterGridAudioProcessorEditor (UTLMeterGridAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.MeterGrid", "Meter Grid")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    heroVisual = std::make_unique<MeterGridVisualComponent> (processorRef, accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);
    addAndMakeVisible (heroVisual.get());

    configureRotarySlider (integrationSlider, "Integration");
    configureRotarySlider (peakHoldSlider, "Peak Hold");
    configureLinearSlider (inputTrimSlider, "Input Trim");
    configureLinearSlider (outputTrimSlider, "Output Trim");
    configureToggle (freezeButton, "Freeze");
    configureToggle (bypassButton, "Soft Bypass");
    configureComboBox (scaleSelector, "Scale");

    scaleSelector.addItem ("Full Scale", 1);
    scaleSelector.addItem ("K-12", 2);
    scaleSelector.addItem ("K-14", 3);
    scaleSelector.addItem ("K-20", 4);
    scaleSelector.setJustificationType (juce::Justification::centred);

    auto& state = processorRef.getValueTreeState();
    const std::initializer_list<std::pair<juce::Slider*, const char*>> sliderPairs
    {
        { &integrationSlider, kParamIntegration },
        { &peakHoldSlider,    kParamPeakHold },
        { &inputTrimSlider,   kParamInputTrim },
        { &outputTrimSlider,  kParamOutputTrim }
    };

    for (const auto& pair : sliderPairs)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, pair.second, *pair.first));

    const std::initializer_list<std::pair<juce::ToggleButton*, const char*>> buttonPairs
    {
        { &freezeButton, kParamFreeze },
        { &bypassButton, kParamBypass }
    };

    for (const auto& pair : buttonPairs)
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, pair.second, *pair.first));

    scaleAttachment = std::make_unique<ComboAttachment> (state, kParamScalePreset, scaleSelector);

    addAndMakeVisible (integrationSlider);
    addAndMakeVisible (peakHoldSlider);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);
    addAndMakeVisible (freezeButton);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (scaleSelector);

    setSize (880, 520);
}

UTLMeterGridAudioProcessorEditor::~UTLMeterGridAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void UTLMeterGridAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
}

void UTLMeterGridAudioProcessorEditor::resized()
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
        &integrationSlider, &peakHoldSlider, &scaleSelector
    });

    auto linearArea = microArea.removeFromTop ((int) (microArea.getHeight() * 0.6f));
    layoutColumn (linearArea,
    {
        &inputTrimSlider, &outputTrimSlider
    });

    auto toggleArea = microArea.reduced (8);
    const int toggleHeight = 34;
    freezeButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    bypassButton.setBounds (toggleArea.removeFromTop (toggleHeight));
}

void UTLMeterGridAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, const juce::String& labelText)
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

void UTLMeterGridAudioProcessorEditor::configureLinearSlider (juce::Slider& slider, const juce::String& labelText)
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

void UTLMeterGridAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setColour (juce::ToggleButton::tickColourId, accentColour);
}

void UTLMeterGridAudioProcessorEditor::configureComboBox (juce::ComboBox& box, const juce::String& labelText)
{
    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (12.0f));
    label->attachToComponent (&box, false);
    labels.push_back (std::move (label));
}

juce::AudioProcessorEditor* UTLMeterGridAudioProcessor::createEditor()
{
    return new UTLMeterGridAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLMeterGridAudioProcessor();
}

int UTLMeterGridAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String UTLMeterGridAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void UTLMeterGridAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void UTLMeterGridAudioProcessor::applyPreset (int index)
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

    if (auto* presetParam = apvts.getParameter (kParamScalePreset))
        scalePreset.store ((int) presetParam->convertFrom0to1 (presetParam->getValue()), std::memory_order_relaxed);
}
