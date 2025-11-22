#include "GRDIronBusAudioProcessor.h"

GRDIronBusAudioProcessor::GRDIronBusAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IRON_BUS", createParameterLayout())
{
}

void GRDIronBusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
}

void GRDIronBusAudioProcessor::releaseResources()
{
}

void GRDIronBusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float drive   = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float glue    = juce::jlimit (0.0f, 1.0f, get ("glue"));
    const float hpf     = juce::jlimit (20.0f, 200.0f, get ("hpf"));
    const float tilt    = juce::jlimit (-1.0f, 1.0f, get ("tilt"));
    const float mix     = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float trimDb  = juce::jlimit (-12.0f, 12.0f, get ("output_trim"));
    const float trimGain= juce::Decibels::decibelsToGain (trimDb);

    ensureStateSize (numChannels);
    updateFilters (hpf, tilt);

    const float driveGain = 1.0f + drive * 8.0f;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer, true);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            sample = state.hpFilter.processSample (sample);
            sample = state.tiltFilter.processSample (sample);

            const float clipped = std::tanh (sample * driveGain);
            const float blended = juce::jmap (glue, sample, clipped);
            data[i] = juce::jmap (mix, dry.getSample (ch, i), blended) * trimGain;
        }
    }
}

void GRDIronBusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDIronBusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDIronBusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("glue", "Glue",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf", "High-Pass",
                                                                   juce::NormalisableRange<float> (20.0f, 200.0f, 0.01f, 0.4f), 70.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt", "Tilt",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void GRDIronBusAudioProcessor::ensureStateSize (int numChannels)
{
    if ((int) channelState.size() < numChannels)
        channelState.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (filterSpecSampleRate, currentSampleRate)
                             || filterSpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& state : channelState)
        {
            state.hpFilter.prepare (spec);
            state.hpFilter.reset();
            state.tiltFilter.prepare (spec);
            state.tiltFilter.reset();
        }
        filterSpecSampleRate = currentSampleRate;
        filterSpecBlockSize  = targetBlock;
    }
}

void GRDIronBusAudioProcessor::updateFilters (float hpfHz, float tilt)
{
    if (currentSampleRate <= 0.0)
        return;

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                 juce::jlimit (20.0f, 250.0f, hpfHz), 0.7f);
    const float tiltFreq = juce::jmap (tilt, -1.0f, 1.0f, 600.0f, 6000.0f);
    auto tiltCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                        juce::jlimit (200.0f, (float) (currentSampleRate * 0.49f), tiltFreq), 0.5f);

    for (auto& state : channelState)
    {
        state.hpFilter.coefficients = hp;
        state.tiltFilter.coefficients = tiltCoeffs;
    }
}

GRDIronBusAudioProcessorEditor::GRDIronBusAudioProcessorEditor (GRDIronBusAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };
    make (driveSlider, "Drive");
    make (glueSlider,  "Glue");
    make (hpfSlider,   "High-Pass");
    make (tiltSlider,  "Tilt");
    make (mixSlider,   "Mix");
    make (trimSlider,  "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "drive", "glue", "hpf", "tilt", "mix", "output_trim" };
    juce::Slider* sliders[] = { &driveSlider, &glueSlider, &hpfSlider, &tiltSlider, &mixSlider, &trimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 280);
}

void GRDIronBusAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GRDIronBusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Iron Bus", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GRDIronBusAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 6;

    driveSlider.setBounds (area.removeFromLeft (width).reduced (8));
    glueSlider .setBounds (area.removeFromLeft (width).reduced (8));
    hpfSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    tiltSlider .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    trimSlider .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GRDIronBusAudioProcessor::createEditor()
{
    return new GRDIronBusAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDIronBusAudioProcessor();
}
