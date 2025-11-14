#include "EQHarmonicEQAudioProcessor.h"

EQHarmonicEQAudioProcessor::EQHarmonicEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HARMONIC_EQ", createParameterLayout())
{
}

void EQHarmonicEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& band : harmonicBands)
    {
        band.base.prepare (spec);
        band.base.reset();
        band.harmonic.prepare (spec);
        band.harmonic.reset();
    }
}

void EQHarmonicEQAudioProcessor::releaseResources()
{
}

void EQHarmonicEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto freq     = get ("band_freq");
    const auto gainDb   = get ("band_gain");
    const auto q        = get ("band_q");
    const auto harmType = static_cast<int> (apvts.getRawParameterValue ("harm_type")->load());
    const auto mix      = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);
    updateFilters (freq, q, gainDb, harmType);

    const float harmonicBlend = harmType == 0 ? 0.6f : (harmType == 1 ? 0.5f : 0.4f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* dry  = dryBuffer.getReadPointer (ch);
        auto& band = harmonicBands[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float baseSample = band.base.processSample (dry[i]);
            const float harmonicSample = band.harmonic.processSample (dry[i]);
            const float combined = juce::jlimit (-2.0f, 2.0f, baseSample + harmonicBlend * harmonicSample);
            data[i] = combined * mix + dry[i] * (1.0f - mix);
        }
    }
}

void EQHarmonicEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQHarmonicEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQHarmonicEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band_freq", "Band Freq",
                                                                   juce::NormalisableRange<float> (40.0f, 20000.0f, 0.01f, 0.4f), 2000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band_gain", "Band Gain",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band_q",    "Band Q",
                                                                   juce::NormalisableRange<float> (0.2f, 10.0f, 0.001f, 0.5f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("harm_type","Harm Type",
                                                                    juce::StringArray { "Odd", "Even", "Hybrid" }, 2));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",       "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

EQHarmonicEQAudioProcessorEditor::EQHarmonicEQAudioProcessorEditor (EQHarmonicEQAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (bandFreqSlider, "Band Freq");
    initSlider (bandGainSlider, "Band Gain");
    initSlider (bandQSlider,    "Band Q");
    initSlider (mixSlider,      "Mix");

    harmTypeBox.addItemList ({ "Odd", "Even", "Hybrid" }, 1);
    addAndMakeVisible (harmTypeBox);

    auto& state = processorRef.getValueTreeState();
    bandFreqAttachment = std::make_unique<SliderAttachment>   (state, "band_freq", bandFreqSlider);
    bandGainAttachment = std::make_unique<SliderAttachment>   (state, "band_gain", bandGainSlider);
    bandQAttachment    = std::make_unique<SliderAttachment>   (state, "band_q",    bandQSlider);
    harmTypeAttachment = std::make_unique<ComboBoxAttachment> (state, "harm_type", harmTypeBox);
    mixAttachment      = std::make_unique<SliderAttachment>   (state, "mix",       mixSlider);

    setSize (640, 260);
}

void EQHarmonicEQAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void EQHarmonicEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Harmonic EQ", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQHarmonicEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 4;

    bandFreqSlider.setBounds (area.removeFromLeft (width).reduced (8));
    bandGainSlider.setBounds (area.removeFromLeft (width).reduced (8));
    bandQSlider   .setBounds (area.removeFromLeft (width).reduced (8));

    auto bottom = area.removeFromTop (80);
    harmTypeBox .setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (8));
    mixSlider   .setBounds (bottom.reduced (8));
}

juce::AudioProcessorEditor* EQHarmonicEQAudioProcessor::createEditor()
{
    return new EQHarmonicEQAudioProcessorEditor (*this);
}

void EQHarmonicEQAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) harmonicBands.size() < numChannels)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        const auto previous = (int) harmonicBands.size();
        harmonicBands.resize (numChannels);

        for (int ch = previous; ch < numChannels; ++ch)
        {
            harmonicBands[ch].base.prepare (spec);
            harmonicBands[ch].base.reset();
            harmonicBands[ch].harmonic.prepare (spec);
            harmonicBands[ch].harmonic.reset();
        }
    }
}

void EQHarmonicEQAudioProcessor::updateFilters (float freq, float q, float gainDb, int harmType)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clampedFreq = juce::jlimit (40.0f, (float) (currentSampleRate * 0.45f), freq);
    const auto clampedQ    = juce::jlimit (0.2f, 10.0f, q);
    const auto gainLinear  = juce::Decibels::decibelsToGain (gainDb);

    float harmonicMultiple = harmType == 0 ? 3.0f : (harmType == 1 ? 2.0f : 2.5f);
    const auto harmonicFreq = juce::jlimit (clampedFreq, (float) (currentSampleRate * 0.49f), clampedFreq * harmonicMultiple);
    const auto harmonicQ    = clampedQ * 0.7f;

    auto baseCoeffs     = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, clampedFreq, clampedQ, gainLinear);
    auto harmonicCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, harmonicFreq, harmonicQ, gainLinear);

    for (auto& band : harmonicBands)
    {
        band.base.coefficients = baseCoeffs;
        band.harmonic.coefficients = harmonicCoeffs;
    }
}
