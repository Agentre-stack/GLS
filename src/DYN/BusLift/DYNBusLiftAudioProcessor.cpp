#include "DYNBusLiftAudioProcessor.h"

DYNBusLiftAudioProcessor::DYNBusLiftAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BUS_LIFT", createParameterLayout())
{
}

void DYNBusLiftAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    auto resetFilter = [sampleRate](auto& filter, juce::dsp::LinkwitzRileyFilterType type, float freq)
    {
        filter.setType (type);
        filter.setCutoffFrequency (freq);
        filter.reset();
        juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
        filter.prepare (spec);
    };

    resetFilter (lowLowpass,  juce::dsp::LinkwitzRileyFilterType::lowpass, 200.0f);
    resetFilter (lowHighpass, juce::dsp::LinkwitzRileyFilterType::highpass, 200.0f);
    resetFilter (midLowpass,  juce::dsp::LinkwitzRileyFilterType::lowpass, 2000.0f);
    resetFilter (midHighpass, juce::dsp::LinkwitzRileyFilterType::highpass, 200.0f);
    resetFilter (highLowpass, juce::dsp::LinkwitzRileyFilterType::lowpass, 20000.0f);
    resetFilter (highHighpass, juce::dsp::LinkwitzRileyFilterType::highpass, 2000.0f);
}

void DYNBusLiftAudioProcessor::releaseResources()
{
}

void DYNBusLiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto lowThresh  = read ("low_thresh");
    const auto midThresh  = read ("mid_thresh");
    const auto highThresh = read ("high_thresh");
    const auto ratio      = juce::jmax (1.0f, read ("ratio"));
    const auto attack     = read ("attack");
    const auto release    = read ("release");
    const auto mix        = juce::jlimit (0.0f, 1.0f, read ("mix"));

    dryBuffer.makeCopyOf (buffer, true);
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    lowBuffer.setSize (numChannels, numSamples, false, false, true);
    midBuffer.setSize (numChannels, numSamples, false, false, true);
    highBuffer.setSize (numChannels, numSamples, false, false, true);

    lowBuffer.makeCopyOf (buffer);
    midBuffer.makeCopyOf (buffer);
    highBuffer.makeCopyOf (buffer);

    juce::dsp::AudioBlock<float> lowBlock (lowBuffer);
    juce::dsp::AudioBlock<float> midBlock (midBuffer);
    juce::dsp::AudioBlock<float> highBlock (highBuffer);
    lowLowpass.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    lowHighpass.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    midLowpass.process (juce::dsp::ProcessContextReplacing<float> (midBlock));
    midHighpass.process (juce::dsp::ProcessContextReplacing<float> (midBlock));
    highHighpass.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

    processBand (lowBuffer, lowThresh, ratio, attack, release);
    processBand (midBuffer, midThresh, ratio, attack, release);
    processBand (highBuffer, highThresh, ratio, attack, release);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        const auto* low = lowBuffer.getReadPointer (ch);
        const auto* mid = midBuffer.getReadPointer (ch);
        const auto* high = highBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float processed = low[i] + mid[i] + high[i];
            out[i] = processed * mix + dry[i] * (1.0f - mix);
        }
    }
}

void DYNBusLiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNBusLiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNBusLiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto threshRange = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto timeRange   = juce::NormalisableRange<float> (1.0f, 100.0f, 0.01f, 0.35f);
    auto releaseRange= juce::NormalisableRange<float> (10.0f, 600.0f, 0.01f, 0.35f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_thresh",  "Low Thresh",  threshRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mid_thresh",  "Mid Thresh",  threshRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_thresh", "High Thresh", threshRange, -12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",       "Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f, 0.5f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",      "Attack", timeRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",     "Release", releaseRange, 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",         "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

DYNBusLiftAudioProcessorEditor::DYNBusLiftAudioProcessorEditor (DYNBusLiftAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto init = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    init (lowThreshSlider,  "Low Thresh");
    init (midThreshSlider,  "Mid Thresh");
    init (highThreshSlider, "High Thresh");
    init (ratioSlider,      "Ratio");
    init (attackSlider,     "Attack");
    init (releaseSlider,    "Release");
    init (mixSlider,        "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "low_thresh", "mid_thresh", "high_thresh", "ratio", "attack", "release", "mix" };
    juce::Slider* sliders[] = { &lowThreshSlider, &midThreshSlider, &highThreshSlider,
                                &ratioSlider, &attackSlider, &releaseSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (780, 300);
}

void DYNBusLiftAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNBusLiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Bus Lift", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNBusLiftAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto row = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (row, { &lowThreshSlider, &midThreshSlider, &highThreshSlider, &ratioSlider });
    layoutRow (area, { &attackSlider, &releaseSlider, &mixSlider });
}

juce::AudioProcessorEditor* DYNBusLiftAudioProcessor::createEditor()
{
    return new DYNBusLiftAudioProcessorEditor (*this);
}

void DYNBusLiftAudioProcessor::processBand (juce::AudioBuffer<float>& bandBuffer, float thresholdDb, float ratio,
                                            float attackMs, float releaseMs)
{
    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto thresholdGain = juce::Decibels::decibelsToGain (thresholdDb);

    for (int ch = 0; ch < bandBuffer.getNumChannels(); ++ch)
    {
        float envelope = 0.0f;
        auto* data = bandBuffer.getWritePointer (ch);
        for (int i = 0; i < bandBuffer.getNumSamples(); ++i)
        {
            const float sample = data[i];
            const float level = std::abs (sample);
            if (level > envelope)
                envelope = attackCoeff * envelope + (1.0f - attackCoeff) * level;
            else
                envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * level;

            float gain = 1.0f;
            if (envelope > thresholdGain)
            {
                const auto envDb = juce::Decibels::gainToDecibels (envelope);
                const auto compressed = thresholdDb + (envDb - thresholdDb) / ratio;
                gain = juce::Decibels::decibelsToGain (compressed - envDb);
            }

            data[i] *= gain;
        }
    }
}
