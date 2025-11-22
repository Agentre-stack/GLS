#include "PITMicroShiftAudioProcessor.h"

namespace
{
constexpr auto kDetuneLId = "detune_l";
constexpr auto kDetuneRId = "detune_r";
constexpr auto kDelayLId  = "delay_l";
constexpr auto kDelayRId  = "delay_r";
constexpr auto kWidthId   = "width";
constexpr auto kHpfId     = "hpf";
constexpr auto kMixId     = "mix";
} // namespace

PITMicroShiftAudioProcessor::PITMicroShiftAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PIT_MICRO_SHIFT", createParameterLayout())
{
}

void PITMicroShiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto totalChannels = juce::jmax (2, getTotalNumOutputChannels());
    const auto blockSize = juce::jmax (1, samplesPerBlock);

    dryBuffer.setSize (totalChannels, blockSize);
    wetBuffer.setSize (totalChannels, blockSize);

    juce::dsp::ProcessSpec chorusSpec { currentSampleRate, static_cast<juce::uint32> (blockSize), 1 };
    for (auto& chorus : chorusProcessors)
    {
        chorus.reset();
        chorus.prepare (chorusSpec);
        chorus.setFeedback (0.0f);
        chorus.setMix (1.0f);
    }

    juce::dsp::ProcessSpec filterSpec { currentSampleRate,
                                        static_cast<juce::uint32> (blockSize),
                                        static_cast<juce::uint32> (totalChannels) };
    hpfProcessor.prepare (filterSpec);
    hpfProcessor.reset();
    updateHighPass (lastHpfCutoff);
}

void PITMicroShiftAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
    wetBuffer.setSize (0, 0);
}

bool PITMicroShiftAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto mainInput  = layouts.getMainInputChannelSet();
    auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != juce::AudioChannelSet::mono() && mainInput != juce::AudioChannelSet::stereo())
        return false;

    if (mainInput != mainOutput)
        return false;

    return true;
}

void PITMicroShiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples        = buffer.getNumSamples();

    for (auto ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto channelCount = juce::jmax (1, buffer.getNumChannels());
    const auto samples = juce::jmax (1, numSamples);
    dryBuffer.setSize (channelCount, samples, false, false, true);
    wetBuffer.setSize (channelCount, samples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);

    const auto detuneL = apvts.getRawParameterValue (kDetuneLId)->load();
    const auto detuneR = apvts.getRawParameterValue (kDetuneRId)->load();
    const auto delayL  = apvts.getRawParameterValue (kDelayLId)->load();
    const auto delayR  = apvts.getRawParameterValue (kDelayRId)->load();
    const auto width   = apvts.getRawParameterValue (kWidthId)->load();
    const auto hpf     = apvts.getRawParameterValue (kHpfId)->load();
    const auto mix     = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (kMixId)->load());

    const auto configureChorus = [] (juce::dsp::Chorus<float>& chorus, float detune, float delayMs)
    {
        const auto detuneAmount = std::abs (detune);
        const auto depth = juce::jmap (detuneAmount, 0.0f, 20.0f, 0.02f, 0.45f);
        const auto rate  = juce::jmap (detuneAmount, 0.0f, 20.0f, 0.08f, 1.5f);
        chorus.setDepth (depth);
        chorus.setRate (rate);
        chorus.setCentreDelay (juce::jlimit (1.0f, 40.0f, delayMs));
    };

    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    if (wetBuffer.getNumChannels() > 0)
    {
        configureChorus (chorusProcessors[0], detuneL, delayL);
        auto leftBlock = wetBlock.getSingleChannelBlock (0);
        chorusProcessors[0].process (juce::dsp::ProcessContextReplacing<float> (leftBlock));
    }

    if (wetBuffer.getNumChannels() > 1)
    {
        configureChorus (chorusProcessors[1], detuneR, delayR);
        auto rightBlock = wetBlock.getSingleChannelBlock (1);
        chorusProcessors[1].process (juce::dsp::ProcessContextReplacing<float> (rightBlock));
    }

    updateHighPass (hpf);
    juce::dsp::ProcessContextReplacing<float> filterCtx (wetBlock);
    hpfProcessor.process (filterCtx);

    processStereoWidth (width, numSamples);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
        buffer.applyGain (ch, 0, numSamples, 1.0f - mix);
        buffer.addFrom (ch, 0, wetBuffer, ch, 0, numSamples, mix);
    }
}

juce::AudioProcessorEditor* PITMicroShiftAudioProcessor::createEditor()
{
    return new PITMicroShiftAudioProcessorEditor (*this);
}

void PITMicroShiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void PITMicroShiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout PITMicroShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kDetuneLId, "Detune L",
                                                                   juce::NormalisableRange<float> (-20.0f, 20.0f, 0.01f), -6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kDetuneRId, "Detune R",
                                                                   juce::NormalisableRange<float> (-20.0f, 20.0f, 0.01f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kDelayLId, "Delay L (ms)",
                                                                   juce::NormalisableRange<float> (0.0f, 30.0f, 0.01f), 8.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kDelayRId, "Delay R (ms)",
                                                                   juce::NormalisableRange<float> (0.0f, 30.0f, 0.01f), 12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kWidthId, "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kHpfId, "HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 1000.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kMixId, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
PITMicroShiftAudioProcessorEditor::PITMicroShiftAudioProcessorEditor (PITMicroShiftAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    setSize (520, 320);

    initSlider (detuneLSlider, "Detune L");
    initSlider (detuneRSlider, "Detune R");
    initSlider (delayLSlider,  "Delay L");
    initSlider (delayRSlider,  "Delay R");
    initSlider (widthSlider,   "Width");
    initSlider (hpfSlider,     "HPF");
    initSlider (mixSlider,     "Mix");

    auto& vts = processorRef.getValueTreeState();
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kDetuneLId, detuneLSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kDetuneRId, detuneRSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kDelayLId,  delayLSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kDelayRId,  delayRSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kWidthId,   widthSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kHpfId,     hpfSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kMixId,     mixSlider));
}

void PITMicroShiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::dimgrey);
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawFittedText ("PIT Micro Shift", getLocalBounds().removeFromTop (30), juce::Justification::centred, 1);
}

void PITMicroShiftAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (40);

    auto row = area.removeFromTop (120);
    auto width = row.getWidth() / 4;
    detuneLSlider.setBounds (row.removeFromLeft (width).reduced (6));
    detuneRSlider.setBounds (row.removeFromLeft (width).reduced (6));
    delayLSlider .setBounds (row.removeFromLeft (width).reduced (6));
    delayRSlider .setBounds (row.removeFromLeft (width).reduced (6));

    row = area.removeFromTop (120);
    width = row.getWidth() / 3;
    widthSlider.setBounds (row.removeFromLeft (width).reduced (6));
    hpfSlider  .setBounds (row.removeFromLeft (width).reduced (6));
    mixSlider  .setBounds (row.removeFromLeft (width).reduced (6));
}

void PITMicroShiftAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PITMicroShiftAudioProcessor();
}

void PITMicroShiftAudioProcessor::updateHighPass (float cutoffHz)
{
    const auto limitedCutoff = juce::jlimit (20.0f, 2000.0f, cutoffHz);
    if (currentSampleRate <= 0.0)
        return;

    if (hpfProcessor.state != nullptr
        && std::abs (limitedCutoff - lastHpfCutoff) < 1.0f)
        return;

    *hpfProcessor.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, limitedCutoff);
    lastHpfCutoff = limitedCutoff;
}

void PITMicroShiftAudioProcessor::processStereoWidth (float widthValue, int numSamples)
{
    if (wetBuffer.getNumChannels() < 2)
        return;

    const auto width = juce::jlimit (0.0f, 1.0f, widthValue);
    auto* left  = wetBuffer.getWritePointer (0);
    auto* right = wetBuffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto mid  = 0.5f * (left[i] + right[i]);
        auto side = 0.5f * (left[i] - right[i]);
        side *= width;

        left[i]  = mid + side;
        right[i] = mid - side;
    }
}
