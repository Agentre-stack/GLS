#include "GLSXOverBusAudioProcessor.h"

GLSXOverBusAudioProcessor::GLSXOverBusAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XOVER_BUS", createParameterLayout())
{
}

void GLSXOverBusAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) 512, (juce::uint32) getTotalNumOutputChannels() };

    auto prepareBand = [this, &spec](BandFilters& band, int order)
    {
        prepareFilters (band, order, spec);
    };

    const int order = juce::roundToInt (apvts.getRawParameterValue ("slope")->load());
    prepareBand (lowBand, order);
    prepareBand (midBandLow, order);
    prepareBand (midBandHigh, order);
    prepareBand (highBand, order);
}

void GLSXOverBusAudioProcessor::releaseResources()
{
}

void GLSXOverBusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

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

    juce::dsp::ProcessSpec blockSpec { currentSampleRate, (juce::uint32) buffer.getNumSamples(), (juce::uint32) buffer.getNumChannels() };
    prepareFilters (lowBand, slope, blockSpec);
    prepareFilters (midBandLow, slope, blockSpec);
    prepareFilters (midBandHigh, slope, blockSpec);
    prepareFilters (highBand, slope, blockSpec);

    updateCoefficients (lowBand, split1, true);
    updateCoefficients (midBandLow, split1, false);
    updateCoefficients (midBandHigh, split2, true);
    updateCoefficients (highBand, split2, false);

    ensureBufferSize (buffer.getNumChannels(), buffer.getNumSamples());

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
            out[i] = sample;
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

    return { params.begin(), params.end() };
}

GLSXOverBusAudioProcessorEditor::GLSXOverBusAudioProcessorEditor (GLSXOverBusAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (split1Slider, "Split 1");
    initSlider (split2Slider, "Split 2");
    initSlider (slopeSlider,  "Slope");
    initSlider (outputSlider, "Output");

    addAndMakeVisible (band1SoloButton);
    addAndMakeVisible (band2SoloButton);
    addAndMakeVisible (band3SoloButton);

    auto& state = processorRef.getValueTreeState();
    split1Attachment = std::make_unique<SliderAttachment> (state, "split_freq1", split1Slider);
    split2Attachment = std::make_unique<SliderAttachment> (state, "split_freq2", split2Slider);
    slopeAttachment  = std::make_unique<SliderAttachment> (state, "slope",       slopeSlider);
    band1Attachment  = std::make_unique<ButtonAttachment> (state, "band_solo1", band1SoloButton);
    band2Attachment  = std::make_unique<ButtonAttachment> (state, "band_solo2", band2SoloButton);
    band3Attachment  = std::make_unique<ButtonAttachment> (state, "band_solo3", band3SoloButton);
    outputAttachment = std::make_unique<SliderAttachment> (state, "output_trim", outputSlider);

    setSize (620, 260);
}

void GLSXOverBusAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GLSXOverBusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS XOver Bus", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSXOverBusAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto top = area.removeFromTop (120);

    split1Slider.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (8));
    split2Slider.setBounds (top.reduced (8));

    auto mid = area.removeFromTop (80);
    slopeSlider.setBounds (mid.removeFromLeft (mid.getWidth() / 2).reduced (8));
    outputSlider.setBounds (mid.reduced (8));

    auto bottom = area.removeFromTop (40);
    band1SoloButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 3).reduced (8));
    band2SoloButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (8));
    band3SoloButton.setBounds (bottom.reduced (8));
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
