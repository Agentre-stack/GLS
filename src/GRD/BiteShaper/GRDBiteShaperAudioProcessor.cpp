#include "GRDBiteShaperAudioProcessor.h"

GRDBiteShaperAudioProcessor::GRDBiteShaperAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BITE_SHAPER", createParameterLayout())
{
}

void GRDBiteShaperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
}

void GRDBiteShaperAudioProcessor::releaseResources()
{
}

void GRDBiteShaperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const float bite   = juce::jlimit (0.0f, 1.0f, get ("bite"));
    const float fold   = juce::jlimit (0.0f, 1.0f, get ("fold"));
    const float tone   = juce::jlimit (400.0f, 12000.0f, get ("tone"));
    const float mix    = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float trimDb = juce::jlimit (-12.0f, 12.0f, get ("output_trim"));

    const float trimGain = juce::Decibels::decibelsToGain (trimDb);
    updateToneFilters (tone);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = data[i];
            const float pre = input * (1.0f + bite * 6.0f);

            float shaped = pre;
            const float folded = pre - 2.0f * std::floor ((pre + juce::MathConstants<float>::pi) / (2.0f * juce::MathConstants<float>::pi)) * juce::MathConstants<float>::pi;
            shaped = shaped * (1.0f - fold) + std::sin (folded) * fold;
            shaped = juce::jlimit (-1.0f, 1.0f, std::tanh (shaped));
            shaped = state.toneFilter.processSample (shaped);

            data[i] = juce::jmap (mix, input, shaped) * trimGain;
        }
    }
}

void GRDBiteShaperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDBiteShaperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDBiteShaperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bite", "Bite",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("fold", "Fold",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone", "Tone",
                                                                   juce::NormalisableRange<float> (400.0f, 12000.0f, 0.01f, 0.4f), 3600.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void GRDBiteShaperAudioProcessor::ensureStateSize (int numChannels)
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
            state.toneFilter.prepare (spec);
            state.toneFilter.reset();
        }
        filterSpecSampleRate = currentSampleRate;
        filterSpecBlockSize  = targetBlock;
    }
}

void GRDBiteShaperAudioProcessor::updateToneFilters (float toneHz)
{
    if (currentSampleRate <= 0.0)
        return;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                    juce::jlimit (200.0f, (float) (currentSampleRate * 0.49f), toneHz), 0.7f);
    for (auto& state : channelState)
        state.toneFilter.coefficients = coeffs;
}

GRDBiteShaperAudioProcessorEditor::GRDBiteShaperAudioProcessorEditor (GRDBiteShaperAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };
    make (biteSlider, "Bite");
    make (foldSlider, "Fold");
    make (toneSlider, "Tone");
    make (mixSlider,  "Mix");
    make (trimSlider, "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "bite", "fold", "tone", "mix", "output_trim" };
    juce::Slider* sliders[] = { &biteSlider, &foldSlider, &toneSlider, &mixSlider, &trimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 260);
}

void GRDBiteShaperAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GRDBiteShaperAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Bite Shaper", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GRDBiteShaperAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    biteSlider.setBounds (area.removeFromLeft (width).reduced (8));
    foldSlider.setBounds (area.removeFromLeft (width).reduced (8));
    toneSlider.setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider .setBounds (area.removeFromLeft (width).reduced (8));
    trimSlider.setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GRDBiteShaperAudioProcessor::createEditor()
{
    return new GRDBiteShaperAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDBiteShaperAudioProcessor();
}
