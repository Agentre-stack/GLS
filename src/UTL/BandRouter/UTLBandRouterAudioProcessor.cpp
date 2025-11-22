#include "UTLBandRouterAudioProcessor.h"

namespace
{
constexpr auto kStateId = "BAND_ROUTER";
constexpr auto kParamLowSplit    = "low_split";
constexpr auto kParamHighSplit   = "high_split";
constexpr auto kParamLowLevel    = "low_level";
constexpr auto kParamMidLevel    = "mid_level";
constexpr auto kParamHighLevel   = "high_level";
constexpr auto kParamLowPan      = "low_pan";
constexpr auto kParamMidPan      = "mid_pan";
constexpr auto kParamHighPan     = "high_pan";
constexpr auto kParamSoloLow     = "solo_low";
constexpr auto kParamSoloMid     = "solo_mid";
constexpr auto kParamSoloHigh    = "solo_high";
constexpr auto kParamMix         = "mix";
constexpr auto kParamInputTrim   = "input_trim";
constexpr auto kParamOutputTrim  = "output_trim";
constexpr auto kParamBypass      = "ui_bypass";

class BandEnergyVisualizer : public juce::Component, private juce::Timer
{
public:
    BandEnergyVisualizer (UTLBandRouterAudioProcessor& proc, juce::Colour accentColour)
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

        auto content = bounds.reduced (20.0f);
        const float totalWidth = content.getWidth();
        const float gap = 12.0f;
        const float barWidth = (totalWidth - (gap * 2.0f)) / 3.0f;
        const juce::StringArray labels { "Low", "Mid", "High" };

        for (int i = 0; i < 3; ++i)
        {
            auto barBounds = juce::Rectangle<float> (content.getX() + i * (barWidth + gap),
                                                     content.getY(),
                                                     barWidth,
                                                     content.getHeight() - 24.0f);
            const float value = juce::jlimit (0.0f, 1.0f, processor.getBandMeter (i));

            g.setColour (gls::ui::Colours::outline().withMultipliedAlpha (0.9f));
            g.drawRoundedRectangle (barBounds, 6.0f, 1.4f);

            auto filled = barBounds.withY (barBounds.getBottom() - barBounds.getHeight() * value)
                                   .withHeight (barBounds.getHeight() * value);
            g.setColour (accent.withMultipliedAlpha (0.85f));
            g.fillRoundedRectangle (filled, 6.0f);

            g.setColour (gls::ui::Colours::textSecondary());
            g.setFont (gls::ui::makeFont (12.0f));
            g.drawFittedText (labels[i], barBounds.toNearestInt().translated (0, (int) barBounds.getHeight()),
                              juce::Justification::centred, 1);
        }

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        g.drawFittedText ("Band energy + routing monitor",
                          bounds.toNearestInt().removeFromBottom (24),
                          juce::Justification::centred, 1);
    }

    void timerCallback() override { repaint(); }

private:
    UTLBandRouterAudioProcessor& processor;
    juce::Colour accent;
};
} // namespace

const std::array<UTLBandRouterAudioProcessor::Preset, 3> UTLBandRouterAudioProcessor::presetBank {{
    { "Mix Split", {
        { kParamLowLevel,   0.0f },
        { kParamMidLevel,   0.0f },
        { kParamHighLevel,  0.0f },
        { kParamLowSplit,  120.0f },
        { kParamHighSplit, 3200.0f },
        { kParamLowPan,     0.0f },
        { kParamMidPan,     0.0f },
        { kParamHighPan,    0.0f },
        { kParamMix,        1.0f },
        { kParamInputTrim,  0.0f },
        { kParamOutputTrim, 0.0f },
        { kParamSoloLow,    0.0f },
        { kParamSoloMid,    0.0f },
        { kParamSoloHigh,   0.0f },
        { kParamBypass,     0.0f }
    }},
    { "Wide Mid", {
        { kParamLowLevel,  -1.0f },
        { kParamMidLevel,   0.0f },
        { kParamHighLevel, -0.5f },
        { kParamLowSplit,  150.0f },
        { kParamHighSplit, 2500.0f },
        { kParamLowPan,    -0.2f },
        { kParamMidPan,     0.3f },
        { kParamHighPan,   -0.2f },
        { kParamMix,        1.0f },
        { kParamInputTrim,  0.0f },
        { kParamOutputTrim, 0.0f },
        { kParamSoloLow,    0.0f },
        { kParamSoloMid,    0.0f },
        { kParamSoloHigh,   0.0f },
        { kParamBypass,     0.0f }
    }},
    { "Low Anchor", {
        { kParamLowLevel,   1.5f },
        { kParamMidLevel,  -0.5f },
        { kParamHighLevel, -1.5f },
        { kParamLowSplit,  100.0f },
        { kParamHighSplit, 1800.0f },
        { kParamLowPan,     0.0f },
        { kParamMidPan,     0.2f },
        { kParamHighPan,   -0.2f },
        { kParamMix,        0.9f },
        { kParamInputTrim, -1.0f },
        { kParamOutputTrim, 0.0f },
        { kParamSoloLow,    0.0f },
        { kParamSoloMid,    0.0f },
        { kParamSoloHigh,   0.0f },
        { kParamBypass,     0.0f }
    }}
}};

UTLBandRouterAudioProcessor::UTLBandRouterAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void UTLBandRouterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  (juce::uint32) juce::jmax (1, samplesPerBlock),
                                  1 };

    for (auto* bank : { &lowFilters, &highFilters })
        for (auto& filter : *bank)
        {
            filter.reset();
            filter.prepare (spec);
        }

    updateFilters (apvts.getRawParameterValue (kParamLowSplit)->load(),
                   apvts.getRawParameterValue (kParamHighSplit)->load());
}

void UTLBandRouterAudioProcessor::releaseResources()
{
}

void UTLBandRouterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    auto read = [this](const char* paramId)
    {
        return apvts.getRawParameterValue (paramId)->load();
    };

    const float lowSplit  = read (kParamLowSplit);
    const float highSplit = read (kParamHighSplit);
    updateFilters (lowSplit, highSplit);

    const auto dbToGain = [](float db) { return juce::Decibels::decibelsToGain (db); };
    const float lowGain   = dbToGain (read (kParamLowLevel));
    const float midGain   = dbToGain (read (kParamMidLevel));
    const float highGain  = dbToGain (read (kParamHighLevel));
    const float lowPan    = juce::jlimit (-1.0f, 1.0f, read (kParamLowPan));
    const float midPan    = juce::jlimit (-1.0f, 1.0f, read (kParamMidPan));
    const float highPan   = juce::jlimit (-1.0f, 1.0f, read (kParamHighPan));
    const bool soloLow    = read (kParamSoloLow)  > 0.5f;
    const bool soloMid    = read (kParamSoloMid)  > 0.5f;
    const bool soloHigh   = read (kParamSoloHigh) > 0.5f;
    const bool anySolo    = soloLow || soloMid || soloHigh;
    const float mix       = juce::jlimit (0.0f, 1.0f, read (kParamMix));
    const float inputTrim = dbToGain (read (kParamInputTrim));
    const float outputTrim= dbToGain (read (kParamOutputTrim));

    buffer.applyGain (inputTrim);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    auto applyPan = [](float pan, float& left, float& right)
    {
        const float leftGain  = pan <= 0.0f ? 1.0f : 1.0f - pan;
        const float rightGain = pan >= 0.0f ? 1.0f : 1.0f + pan;
        left *= leftGain;
        right *= rightGain;
    };

    auto accumulateBand = [anySolo, applyPan](float bandGain, float pan, bool solo,
                                              float& left, float& right)
    {
        if (anySolo && ! solo)
        {
            left = 0.0f;
            right = 0.0f;
            return;
        }

        left *= bandGain;
        right *= bandGain;
        applyPan (pan, left, right);
    };

    auto& lowFilterL  = lowFilters[0];
    auto& lowFilterR  = lowFilters[juce::jmin (1, numChannels - 1)];
    auto& highFilterL = highFilters[0];
    auto& highFilterR = highFilters[juce::jmin (1, numChannels - 1)];

    auto* leftData  = buffer.getWritePointer (0);
    auto* rightData = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    float lowPeak = 0.0f;
    float midPeak = 0.0f;
    float highPeak= 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inL = leftData[sample];
        const float inR = rightData != nullptr ? rightData[sample] : inL;

        float lowL  = lowFilterL.processSample (inL);
        float lowR  = lowFilterR.processSample (inR);
        float highL = highFilterL.processSample (inL);
        float highR = highFilterR.processSample (inR);
        float midL  = inL - lowL - highL;
        float midR  = inR - lowR - highR;

        lowPeak  = juce::jmax (lowPeak,  juce::jmax (std::abs (lowL),  std::abs (lowR)));
        midPeak  = juce::jmax (midPeak,  juce::jmax (std::abs (midL),  std::abs (midR)));
        highPeak = juce::jmax (highPeak, juce::jmax (std::abs (highL), std::abs (highR)));

        float bandLowL = lowL,  bandLowR = lowR;
        float bandMidL = midL,  bandMidR = midR;
        float bandHighL= highL, bandHighR= highR;

        accumulateBand (lowGain,  lowPan,  soloLow,  bandLowL,  bandLowR);
        accumulateBand (midGain,  midPan,  soloMid,  bandMidL,  bandMidR);
        accumulateBand (highGain, highPan, soloHigh, bandHighL, bandHighR);

        float outL = bandLowL + bandMidL + bandHighL;
        float outR = bandLowR + bandMidR + bandHighR;

        leftData[sample] = outL;
        if (rightData != nullptr)
            rightData[sample] = outR;
        else
            leftData[sample] = 0.5f * (outL + outR);
    }

    auto smoothMeter = [](std::atomic<float>& target, float newValue)
    {
        const float previous = target.load();
        const float blended = previous * 0.85f + newValue * 0.15f;
        target.store (juce::jlimit (0.0f, 1.0f, blended));
    };

    smoothMeter (bandMeters[0], juce::jlimit (0.0f, 1.0f, lowPeak));
    smoothMeter (bandMeters[1], juce::jlimit (0.0f, 1.0f, midPeak));
    smoothMeter (bandMeters[2], juce::jlimit (0.0f, 1.0f, highPeak));

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

    buffer.applyGain (outputTrim);
}

float UTLBandRouterAudioProcessor::getBandMeter (int bandIndex) const noexcept
{
    if (juce::isPositiveAndBelow (bandIndex, (int) bandMeters.size()))
        return bandMeters[(size_t) bandIndex].load();
    return 0.0f;
}

void UTLBandRouterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void UTLBandRouterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLBandRouterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLowSplit, "Low Split",
                                                                   juce::NormalisableRange<float> (80.0f, 400.0f, 0.01f, 0.45f), 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHighSplit, "High Split",
                                                                   juce::NormalisableRange<float> (600.0f, 6000.0f, 0.01f, 0.45f), 2500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLowLevel, "Low Level",
                                                                   juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMidLevel, "Mid Level",
                                                                   juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHighLevel, "High Level",
                                                                   juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLowPan, "Low Pan",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMidPan, "Mid Pan",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHighPan, "High Pan",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamSoloLow,  "Solo Low",  false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamSoloMid,  "Solo Mid",  false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamSoloHigh, "Solo High", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void UTLBandRouterAudioProcessor::updateFilters (float lowHz, float highHz)
{
    if (currentSampleRate <= 0.0)
        return;

    const float safeLow  = juce::jlimit (80.0f, 400.0f, lowHz);
    const float minHigh  = safeLow + 200.0f;
    const float safeHigh = juce::jlimit (minHigh, (float) (currentSampleRate * 0.45f), highHz);

    if (juce::approximatelyEqual (safeLow, currentLowSplit)
        && juce::approximatelyEqual (safeHigh, currentHighSplit))
        return;

    auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, safeLow, 0.707f);
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, safeHigh, 0.707f);

    for (auto& filter : lowFilters)
        filter.coefficients = lowCoeffs;
    for (auto& filter : highFilters)
        filter.coefficients = highCoeffs;

    currentLowSplit  = safeLow;
    currentHighSplit = safeHigh;
}

UTLBandRouterAudioProcessorEditor::UTLBandRouterAudioProcessorEditor (UTLBandRouterAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.BandRouter", "Band Router")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    heroVisual = std::make_unique<BandEnergyVisualizer> (processorRef, accentColour);
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

    makeRotary (lowLevelSlider,  "Low Level");
    makeRotary (midLevelSlider,  "Mid Level");
    makeRotary (highLevelSlider, "High Level");
    makeRotary (lowSplitSlider,  "Low Split");
    makeRotary (highSplitSlider, "High Split");
    makeRotary (lowPanSlider,    "Low Pan");
    makeRotary (midPanSlider,    "Mid Pan");
    makeRotary (highPanSlider,   "High Pan");
    makeLinear (mixSlider,       "Mix", true);
    makeLinear (inputTrimSlider, "Input Trim", true);
    makeLinear (outputTrimSlider,"Output Trim", true);

    configureToggle (soloLowButton,  "Solo Low");
    configureToggle (soloMidButton,  "Solo Mid");
    configureToggle (soloHighButton, "Solo High");
    configureToggle (bypassButton,   "Soft Bypass");

    auto& state = processorRef.getValueTreeState();
    const std::initializer_list<std::pair<juce::Slider*, const char*>> sliderPairs =
    {
        { &lowSplitSlider,  kParamLowSplit },
        { &highSplitSlider, kParamHighSplit },
        { &lowLevelSlider,  kParamLowLevel },
        { &midLevelSlider,  kParamMidLevel },
        { &highLevelSlider, kParamHighLevel },
        { &lowPanSlider,    kParamLowPan },
        { &midPanSlider,    kParamMidPan },
        { &highPanSlider,   kParamHighPan },
        { &mixSlider,       kParamMix },
        { &inputTrimSlider, kParamInputTrim },
        { &outputTrimSlider,kParamOutputTrim }
    };

    for (const auto& pair : sliderPairs)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, pair.second, *pair.first));

    const std::initializer_list<std::pair<juce::ToggleButton*, const char*>> buttonPairs =
    {
        { &soloLowButton,  kParamSoloLow },
        { &soloMidButton,  kParamSoloMid },
        { &soloHighButton, kParamSoloHigh },
        { &bypassButton,   kParamBypass }
    };

    for (const auto& pair : buttonPairs)
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, pair.second, *pair.first));

    addAndMakeVisible (soloLowButton);
    addAndMakeVisible (soloMidButton);
    addAndMakeVisible (soloHighButton);
    addAndMakeVisible (bypassButton);

    addAndMakeVisible (lowLevelSlider);
    addAndMakeVisible (midLevelSlider);
    addAndMakeVisible (highLevelSlider);
    addAndMakeVisible (lowSplitSlider);
    addAndMakeVisible (highSplitSlider);
    addAndMakeVisible (lowPanSlider);
    addAndMakeVisible (midPanSlider);
    addAndMakeVisible (highPanSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);

    setSize (920, 600);
}

UTLBandRouterAudioProcessorEditor::~UTLBandRouterAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void UTLBandRouterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
}

void UTLBandRouterAudioProcessorEditor::resized()
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
        &lowLevelSlider, &midLevelSlider, &highLevelSlider,
        &lowSplitSlider, &highSplitSlider
    });

    auto sliderStack = microArea.removeFromTop ((int) (microArea.getHeight() * 0.6f));
    layoutColumn (sliderStack,
    {
        &lowPanSlider, &midPanSlider, &highPanSlider,
        &mixSlider, &inputTrimSlider
    });

    outputTrimSlider.setBounds (microArea.removeFromTop (48).reduced (8));

    auto toggleArea = microArea.reduced (8);
    const int toggleHeight = 32;
    soloLowButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    soloMidButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    soloHighButton.setBounds (toggleArea.removeFromTop (toggleHeight));
    bypassButton.setBounds (toggleArea.removeFromTop (toggleHeight));
}

void UTLBandRouterAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, const juce::String& labelText)
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

void UTLBandRouterAudioProcessorEditor::configureLinearSlider (juce::Slider& slider, const juce::String& labelText, bool isHorizontal)
{
    slider.setSliderStyle (isHorizontal ? juce::Slider::LinearHorizontal
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

void UTLBandRouterAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setColour (juce::ToggleButton::tickColourId, accentColour);
}

juce::AudioProcessorEditor* UTLBandRouterAudioProcessor::createEditor()
{
    return new UTLBandRouterAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLBandRouterAudioProcessor();
}

int UTLBandRouterAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String UTLBandRouterAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void UTLBandRouterAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void UTLBandRouterAudioProcessor::applyPreset (int index)
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
