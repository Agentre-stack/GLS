#include "GROctaneClipperAudioProcessor.h"

GROctaneClipperAudioProcessor::GROctaneClipperAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OCTANE_CLIPPER", createParameterLayout())
{
}

void GROctaneClipperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
}

void GROctaneClipperAudioProcessor::releaseResources()
{
}

void GROctaneClipperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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
    const float clipSel = juce::jlimit (0.0f, 2.0f, get ("clip_type"));
    const float hpCut   = juce::jlimit (20.0f, 200.0f, get ("hp"));
    const float mix     = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float trimDb  = juce::jlimit (-12.0f, 12.0f, get ("output_trim"));
    const float trimGain= juce::Decibels::decibelsToGain (trimDb);

    ensureStateSize (numChannels);
    updateFilters (hpCut);

    const float driveGain = 1.0f + drive * 12.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = data[i];
            float sample = state.hpFilter.processSample (input) * driveGain;
            float clipped = 0.0f;

            if (clipSel < 1.0f)
                clipped = juce::jlimit (-1.0f, 1.0f, sample);
            else if (clipSel < 2.0f)
                clipped = std::tanh (sample);
            else
            {
                const float sign = sample >= 0.0f ? 1.0f : -1.0f;
                clipped = sign * (1.0f - std::exp (-std::abs (sample)));
            }

            data[i] = juce::jmap (mix, input, clipped) * trimGain;
        }
    }
}

void GROctaneClipperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GROctaneClipperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GROctaneClipperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("clip_type", "Clip Type",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hp", "High-Pass",
                                                                   juce::NormalisableRange<float> (20.0f, 200.0f, 0.01f, 0.4f), 60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void GROctaneClipperAudioProcessor::ensureStateSize (int numChannels)
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
        }
        filterSpecSampleRate = currentSampleRate;
        filterSpecBlockSize  = targetBlock;
    }
}

void GROctaneClipperAudioProcessor::updateFilters (float cutoff)
{
    if (currentSampleRate <= 0.0)
        return;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                     juce::jlimit (20.0f, 400.0f, cutoff), 0.7f);
    for (auto& state : channelState)
        state.hpFilter.coefficients = coeffs;
}

GROctaneClipperAudioProcessorEditor::GROctaneClipperAudioProcessorEditor (GROctaneClipperAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };
    make (driveSlider,     "Drive");
    make (clipTypeSlider,  "Clip Type");
    make (hpSlider,        "High-Pass");
    make (mixSlider,       "Mix");
    make (trimSlider,      "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "drive", "clip_type", "hp", "mix", "output_trim" };
    juce::Slider* sliders[] = { &driveSlider, &clipTypeSlider, &hpSlider, &mixSlider, &trimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (660, 260);
}

void GROctaneClipperAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GROctaneClipperAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Octane Clipper", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GROctaneClipperAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    driveSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    clipTypeSlider.setBounds (area.removeFromLeft (width).reduced (8));
    hpSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    trimSlider    .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GROctaneClipperAudioProcessor::createEditor()
{
    return new GROctaneClipperAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GROctaneClipperAudioProcessor();
}
