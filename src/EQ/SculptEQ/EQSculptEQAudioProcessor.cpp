#include "EQSculptEQAudioProcessor.h"

EQSculptEQAudioProcessor::EQSculptEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SCULPT_EQ", createParameterLayout())
{
}

void EQSculptEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureFilterState (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& filter : highPassFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : lowPassFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& band : bandFilters)
        for (auto& filter : band)
        {
            filter.prepare (spec);
            filter.reset();
        }
}

void EQSculptEQAudioProcessor::releaseResources()
{
}

void EQSculptEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    const float hpfFreq = get ("hpf");
    const float lpfFreq = get ("lpf");

    std::array<float, 6> freqs {};
    std::array<float, 6> gains {};
    std::array<float, 6> qs {};
    for (int i = 0; i < 6; ++i)
    {
        const auto idx = juce::String (i + 1);
        freqs[i] = get ("band" + idx + "_freq");
        gains[i] = get ("band" + idx + "_gain");
        qs[i]    = get ("band" + idx + "_q");
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureFilterState (numChannels);
    updateFilters (hpfFreq, lpfFreq, freqs, gains, qs);

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);

        {
            juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
            highPassFilters[ch].process (ctx);
        }
        {
            juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
            lowPassFilters[ch].process (ctx);
        }

        for (auto& band : bandFilters)
        {
            juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
            band[ch].process (ctx);
        }
    }
}

void EQSculptEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQSculptEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQSculptEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf", "HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 200.0f, 0.01f, 0.4f), 40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lpf", "LPF",
                                                                   juce::NormalisableRange<float> (4000.0f, 20000.0f, 0.01f, 0.4f), 16000.0f));

    for (int i = 0; i < 6; ++i)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("band" + juce::String (i+1) + "_freq",
                                                                       "Band" + juce::String (i+1) + " Freq",
                                                                       juce::NormalisableRange<float> (40.0f, 20000.0f, 0.01f, 0.4f), 200.0f * (i + 1)));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("band" + juce::String (i+1) + "_gain",
                                                                       "Band" + juce::String (i+1) + " Gain",
                                                                       juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("band" + juce::String (i+1) + "_q",
                                                                       "Band" + juce::String (i+1) + " Q",
                                                                       juce::NormalisableRange<float> (0.2f, 10.0f, 0.001f, 0.5f), 1.0f));
    }

    return { params.begin(), params.end() };
}

EQSculptEQAudioProcessorEditor::EQSculptEQAudioProcessorEditor (EQSculptEQAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (hpfSlider, "HPF");
    initSlider (lpfSlider, "LPF");

    for (size_t i = 0; i < freqSliders.size(); ++i)
    {
        initSlider (freqSliders[i],  "F" + juce::String ((int) i + 1));
        initSlider (gainSliders[i],  "G" + juce::String ((int) i + 1));
        initSlider (qSliders[i],     "Q" + juce::String ((int) i + 1));
    }

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, "hpf", hpfSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, "lpf", lpfSlider));

    for (size_t i = 0; i < freqSliders.size(); ++i)
    {
        attachments.push_back (std::make_unique<SliderAttachment> (state, "band" + juce::String ((int)i+1) + "_freq", freqSliders[i]));
        attachments.push_back (std::make_unique<SliderAttachment> (state, "band" + juce::String ((int)i+1) + "_gain", gainSliders[i]));
        attachments.push_back (std::make_unique<SliderAttachment> (state, "band" + juce::String ((int)i+1) + "_q",    qSliders[i]));
    }

    setSize (900, 380);
}

void EQSculptEQAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void EQSculptEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Sculpt EQ", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQSculptEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (100);
    hpfSlider.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (8));
    lpfSlider.setBounds (top.reduced (8));

    auto bandHeight = area.getHeight() / 3;
    for (int row = 0; row < 3; ++row)
    {
        auto rowBounds = area.removeFromTop (bandHeight);
        auto width = rowBounds.getWidth() / 6;
        for (int col = 0; col < 6; ++col)
        {
            juce::Slider* slider = nullptr;
            if (row == 0) slider = &freqSliders[col];
            else if (row == 1) slider = &gainSliders[col];
            else slider = &qSliders[col];
            slider->setBounds (rowBounds.removeFromLeft (width).reduced (8));
        }
    }
}

juce::AudioProcessorEditor* EQSculptEQAudioProcessor::createEditor()
{
    return new EQSculptEQAudioProcessorEditor (*this);
}

void EQSculptEQAudioProcessor::ensureFilterState (int numChannels)
{
    if (numChannels <= 0)
        return;

    const auto ensureVector = [this, numChannels](auto& vec)
    {
        if ((int) vec.size() < numChannels)
        {
            juce::dsp::ProcessSpec spec { currentSampleRate,
                                          lastBlockSize > 0 ? lastBlockSize : 512u,
                                          1 };
            const auto previous = (int) vec.size();
            vec.resize (numChannels);
            for (int ch = previous; ch < numChannels; ++ch)
            {
                vec[ch].prepare (spec);
                vec[ch].reset();
            }
        }
    };

    ensureVector (highPassFilters);
    ensureVector (lowPassFilters);
    for (auto& band : bandFilters)
        ensureVector (band);
}

void EQSculptEQAudioProcessor::updateFilters (float hpf, float lpf,
                                              const std::array<float, 6>& freqs,
                                              const std::array<float, 6>& gains,
                                              const std::array<float, 6>& qs)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto hpFreq = juce::jlimit (20.0f, (float) (currentSampleRate * 0.45f), hpf);
    const auto lpFreq = juce::jlimit (hpFreq + 10.0f, (float) (currentSampleRate * 0.49f), lpf);

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpFreq, 0.707f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate, lpFreq, 0.707f);

    for (auto& filter : highPassFilters)
        filter.coefficients = hpCoeffs;
    for (auto& filter : lowPassFilters)
        filter.coefficients = lpCoeffs;

    for (size_t i = 0; i < bandFilters.size(); ++i)
    {
        const auto freq = juce::jlimit (20.0f, (float) (currentSampleRate * 0.49f), freqs[i]);
        const auto q    = juce::jlimit (0.2f, 10.0f, qs[i]);
        const auto gainLinear = juce::Decibels::decibelsToGain (gains[i]);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, freq, q, gainLinear);
        for (auto& filter : bandFilters[i])
            filter.coefficients = coeffs;
    }
}
