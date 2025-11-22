#include "UTLNoiseGenLabAudioProcessor.h"

namespace
{
constexpr auto kStateId        = "NOISE_GEN_LAB";
constexpr auto kParamColour    = "noise_color";
constexpr auto kParamLevel     = "noise_level";
constexpr auto kParamMix       = "mix";
constexpr auto kParamDensity   = "density";
constexpr auto kParamLowCut    = "low_cut";
constexpr auto kParamHighCut   = "high_cut";
constexpr auto kParamStereoVar = "stereo_var";
constexpr auto kParamInputTrim = "input_trim";
constexpr auto kParamOutputTrim= "output_trim";
constexpr auto kParamBypass    = "ui_bypass";

inline int normalisedSamples (double sampleRate, float seconds)
{
    return juce::jmax (8, (int) std::round (seconds * sampleRate));
}

class NoiseEnergyVisual : public juce::Component, private juce::Timer
{
public:
    NoiseEnergyVisual (UTLNoiseGenLabAudioProcessor& processorRef, juce::Colour accentColour)
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
        g.drawRoundedRectangle (bounds, 12.0f, 1.4f);

        auto meterArea = bounds.reduced (24.0f).removeFromLeft (bounds.getWidth() * 0.55f);
        const float energy = juce::jlimit (0.0f, 1.0f, processor.getNoiseMeter());

        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (meterArea, 6.0f, 1.3f);

        auto fill = meterArea;
        fill.removeFromLeft (fill.getWidth() * (1.0f - energy));
        g.setColour (accent.withMultipliedAlpha (0.85f));
        g.fillRoundedRectangle (fill, 6.0f);

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        g.drawFittedText ("Noise energy", meterArea.toNearestInt().translated (0, (int) meterArea.getHeight() + 4),
                          juce::Justification::centred, 1);

        auto infoArea = bounds.removeFromRight (bounds.getWidth() * 0.38f).reduced (12.0f);
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Noise Lab hero mixes white/pink/brown spectra\n"
                          "with burst envelopes + stereo variance.",
                          infoArea.toNearestInt(),
                          juce::Justification::topLeft, 3);
    }

    void timerCallback() override { repaint(); }

private:
    UTLNoiseGenLabAudioProcessor& processor;
    juce::Colour accent;
};
} // namespace

UTLNoiseGenLabAudioProcessor::UTLNoiseGenLabAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
    for (auto& env : burstEnvelopes)
    {
        env.reset (currentSampleRate, 0.01);
        env.setCurrentAndTargetValue (0.0f);
    }

    for (auto& counter : burstCounters)
        counter = 1;
}

void UTLNoiseGenLabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);

    for (auto& state : noiseStates)
        state = {};

    for (auto& filter : lowPassFilters)
        filter.reset();

    for (auto& filter : highPassFilters)
        filter.reset();

    for (auto& env : burstEnvelopes)
    {
        env.reset (currentSampleRate, 0.01);
        env.setCurrentAndTargetValue (0.0f);
    }

    for (auto& counter : burstCounters)
        counter = 1;

    updateFilters (apvts.getRawParameterValue (kParamLowCut)->load(),
                   apvts.getRawParameterValue (kParamHighCut)->load());
}

void UTLNoiseGenLabAudioProcessor::releaseResources()
{
}

void UTLNoiseGenLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float noiseLevelDb = get (kParamLevel);
    const float mix          = juce::jlimit (0.0f, 1.0f, get (kParamMix));
    const float density      = juce::jlimit (0.0f, 1.0f, get (kParamDensity));
    const float stereoVar    = juce::jlimit (0.0f, 1.0f, get (kParamStereoVar));
    const int noiseMode      = (int) std::round (get (kParamColour));
    const float lowCut       = get (kParamLowCut);
    const float highCut      = get (kParamHighCut);
    const float inputTrim    = juce::Decibels::decibelsToGain (get (kParamInputTrim));
    const float outputTrim   = juce::Decibels::decibelsToGain (get (kParamOutputTrim));

    if (std::abs (lowCut - lastLowCut) > 0.5f || std::abs (highCut - lastHighCut) > 0.5f)
        updateFilters (lowCut, highCut);

    buffer.applyGain (inputTrim);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    const float noiseGain = juce::Decibels::decibelsToGain (noiseLevelDb);
    const float dryGain = 1.0f - mix;
    const float wetGain = mix;

    float runningEnergy = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        refreshBurstTargets (density, stereoVar);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int filterIndex = juce::jlimit (0, (int) lowPassFilters.size() - 1, ch);
            const int envIndex = juce::jlimit (0, (int) burstEnvelopes.size() - 1, ch);

            const float dry = dryBuffer.getSample (ch, sample);
            float noise = generateNoise (filterIndex, noiseMode, stereoVar);

            noise = highPassFilters[(size_t) filterIndex].processSample (noise);
            noise = lowPassFilters[(size_t) filterIndex].processSample (noise);

            const float env = burstEnvelopes[(size_t) envIndex].getNextValue();
            const float injected = noise * env * noiseGain;
            const float wet = dry + injected;

            buffer.setSample (ch, sample, dryGain * dry + wetGain * wet);
            runningEnergy += std::abs (injected);
        }
    }

    buffer.applyGain (outputTrim);

    const float averageEnergy = runningEnergy / (float) (numSamples * juce::jmax (1, numChannels));
    const float smoothed = noiseMeter.load() * 0.85f + juce::jlimit (0.0f, 1.0f, averageEnergy) * 0.15f;
    noiseMeter.store (smoothed);
}

void UTLNoiseGenLabAudioProcessor::updateFilters (float lowCutHz, float highCutHz)
{
    lastLowCut = juce::jlimit (20.0f, 20000.0f, lowCutHz);
    lastHighCut = juce::jlimit (200.0f, 20000.0f, highCutHz);

    if (currentSampleRate <= 0.0)
        return;

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, lastLowCut, 0.707f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, lastHighCut, 0.707f);

    for (auto& filter : highPassFilters)
        filter.coefficients = hp;

    for (auto& filter : lowPassFilters)
        filter.coefficients = lp;
}

float UTLNoiseGenLabAudioProcessor::generateNoise (int channel, int noiseMode, float stereoVariance)
{
    const float white = (random.nextFloat() * 2.0f) - 1.0f;
    auto& state = noiseStates[(size_t) juce::jlimit (0, (int) noiseStates.size() - 1, channel)];

    if (noiseMode == 1)
    {
        state.pink = 0.997f * state.pink + 0.003f * white;
        return state.pink;
    }

    if (noiseMode == 2)
    {
        state.brown = juce::jlimit (-1.0f, 1.0f, state.brown + 0.02f * white);
        return state.brown;
    }

    if (stereoVariance > 0.01f)
        return juce::jlimit (-1.0f, 1.0f, white + ((random.nextFloat() * 2.0f - 1.0f) * stereoVariance * 0.35f));

    return white;
}

void UTLNoiseGenLabAudioProcessor::refreshBurstTargets (float density, float stereoVariance)
{
    if (currentSampleRate <= 0.0)
        return;

    const float minHold = 0.004f;
    const float maxHold = 0.18f;
    const float holdSeconds = juce::jmap (density, minHold, maxHold);
    const int baseSamples = normalisedSamples (currentSampleRate, holdSeconds);

    for (size_t ch = 0; ch < burstCounters.size(); ++ch)
    {
        if (--burstCounters[ch] <= 0)
        {
            const float range = 1.0f + (stereoVariance * (random.nextFloat() - 0.5f));
            burstCounters[ch] = juce::jmax (8, (int) std::round (baseSamples * juce::jlimit (0.3f, 1.7f, range)));

            const float randomValue = juce::jmax (0.0001f, random.nextFloat());
            const float curvature = juce::jmap (density, 1.8f, 0.35f);
            const float target = std::pow (randomValue, juce::jlimit (0.2f, 3.0f, curvature));
            burstEnvelopes[ch].setTargetValue (target);
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLNoiseGenLabAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (kParamColour, "Noise Color",
                                                                    juce::StringArray { "White", "Pink", "Brown" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLevel, "Noise Level",
                                                                   juce::NormalisableRange<float> (-60.0f, 6.0f, 0.01f), -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamDensity, "Density",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamLowCut, "Low Cut",
                                                                   juce::NormalisableRange<float> (20.0f, 4000.0f, 0.01f, 0.4f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamHighCut, "High Cut",
                                                                   juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.01f, 0.4f), 12000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamStereoVar, "Stereo Variance",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

UTLNoiseGenLabAudioProcessorEditor::UTLNoiseGenLabAudioProcessorEditor (UTLNoiseGenLabAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.NoiseGenLab", "Noise Gen Lab")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    heroVisual = std::make_unique<NoiseEnergyVisual> (processorRef, accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);
    addAndMakeVisible (heroVisual.get());

    configureComboBox (colorSelector, "Noise Color");
    colorSelector.addItem ("White", 1);
    colorSelector.addItem ("Pink", 2);
    colorSelector.addItem ("Brown", 3);
    colorSelector.setJustificationType (juce::Justification::centred);

    configureRotarySlider (noiseLevelSlider, "Noise Level");
    configureRotarySlider (mixSlider, "Mix");
    configureRotarySlider (densitySlider, "Density");
    configureRotarySlider (lowCutSlider, "Low Cut");
    configureRotarySlider (highCutSlider, "High Cut");
    configureRotarySlider (stereoVarSlider, "Stereo Var");
    configureLinearSlider (inputTrimSlider, "Input Trim");
    configureLinearSlider (outputTrimSlider, "Output Trim");
    configureToggle (bypassButton, "Soft Bypass");

    auto& state = processorRef.getValueTreeState();

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamLevel,     noiseLevelSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamMix,       mixSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamDensity,   densitySlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamLowCut,    lowCutSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamHighCut,   highCutSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamStereoVar, stereoVarSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamInputTrim, inputTrimSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamOutputTrim,outputTrimSlider));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));
    colorAttachment = std::make_unique<ComboAttachment> (state, kParamColour, colorSelector);

    addAndMakeVisible (colorSelector);
    addAndMakeVisible (noiseLevelSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (densitySlider);
    addAndMakeVisible (lowCutSlider);
    addAndMakeVisible (highCutSlider);
    addAndMakeVisible (stereoVarSlider);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);
    addAndMakeVisible (bypassButton);

    setSize (900, 520);
}

UTLNoiseGenLabAudioProcessorEditor::~UTLNoiseGenLabAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void UTLNoiseGenLabAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
}

void UTLNoiseGenLabAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerArea = bounds.removeFromTop (72);
    auto footerArea = bounds.removeFromBottom (72);
    headerComponent.setBounds (headerArea);
    footerComponent.setBounds (footerArea);

    auto body = bounds.reduced (16);
    auto macroArea = body.removeFromLeft ((int) (body.getWidth() * 0.32f)).reduced (8);
    auto heroArea  = body.removeFromLeft ((int) (body.getWidth() * 0.42f)).reduced (8);
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
        &colorSelector, &noiseLevelSlider, &mixSlider, &densitySlider
    });

    layoutColumn (microArea,
    {
        &lowCutSlider, &highCutSlider, &stereoVarSlider,
        &inputTrimSlider, &outputTrimSlider
    });

    bypassButton.setBounds (microArea.removeFromBottom (42).reduced (12));
}

void UTLNoiseGenLabAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, const juce::String& labelText)
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

void UTLNoiseGenLabAudioProcessorEditor::configureLinearSlider (juce::Slider& slider, const juce::String& labelText)
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

void UTLNoiseGenLabAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setColour (juce::ToggleButton::tickColourId, accentColour);
}

void UTLNoiseGenLabAudioProcessorEditor::configureComboBox (juce::ComboBox& box, const juce::String& text)
{
    auto label = std::make_unique<juce::Label>();
    label->setText (text, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (12.0f));
    label->attachToComponent (&box, false);
    labels.push_back (std::move (label));
}

juce::AudioProcessorEditor* UTLNoiseGenLabAudioProcessor::createEditor()
{
    return new UTLNoiseGenLabAudioProcessorEditor (*this);
}

void UTLNoiseGenLabAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void UTLNoiseGenLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLNoiseGenLabAudioProcessor();
}
