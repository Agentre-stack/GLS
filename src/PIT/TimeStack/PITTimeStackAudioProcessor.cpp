#include "PITTimeStackAudioProcessor.h"

namespace
{
constexpr auto kTapTimeIds   = std::array<const char*, 4> { "tap1_time", "tap2_time", "tap3_time", "tap4_time" };
constexpr auto kTapLevelIds  = std::array<const char*, 4> { "tap1_level", "tap2_level", "tap3_level", "tap4_level" };
constexpr auto kTapPanIds    = std::array<const char*, 4> { "tap1_pan", "tap2_pan", "tap3_pan", "tap4_pan" };
constexpr auto kHpfId        = "hpf";
constexpr auto kLpfId        = "lpf";
constexpr auto kSwingId      = "swing";
constexpr auto kMixId        = "mix";
} // namespace

//==============================================================================
PITTimeStackAudioProcessor::PITTimeStackAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PIT_TIME_STACK", createParameterLayout())
{
}

void PITTimeStackAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto totalChannels = juce::jmax (2, getTotalNumOutputChannels());
    const auto blockSize = juce::jmax (1, samplesPerBlock);

    dryBuffer.setSize (totalChannels, blockSize);
    monoBuffer.setSize (1, blockSize);
    tapScratchBuffer.setSize (1, blockSize);
    wetBuffer.setSize (totalChannels, blockSize);

    const auto maxDelaySamples = juce::jmax (1, (int) std::ceil (currentSampleRate * 3.0));
    juce::dsp::ProcessSpec monoSpec { currentSampleRate, static_cast<juce::uint32> (blockSize), 1 };

    for (auto& line : tapDelayLines)
    {
        line.prepare (monoSpec);
        line.setMaximumDelayInSamples (maxDelaySamples);
        line.reset();
    }

    juce::dsp::ProcessSpec stereoSpec { currentSampleRate,
                                        static_cast<juce::uint32> (blockSize),
                                        static_cast<juce::uint32> (totalChannels) };
    hpfProcessor.prepare (stereoSpec);
    lpfProcessor.prepare (stereoSpec);
    hpfProcessor.reset();
    lpfProcessor.reset();
    updateFilters (lastHpfCutoff, lastLpfCutoff);
}

void PITTimeStackAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
    monoBuffer.setSize (0, 0);
    tapScratchBuffer.setSize (0, 0);
    wetBuffer.setSize (0, 0);
}

bool PITTimeStackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto mainInput  = layouts.getMainInputChannelSet();
    auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != juce::AudioChannelSet::mono() && mainInput != juce::AudioChannelSet::stereo())
        return false;

    if (mainInput != mainOutput)
        return false;

    return true;
}

void PITTimeStackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples        = buffer.getNumSamples();

    for (auto ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    ensureBuffers (buffer.getNumChannels(), numSamples);
    dryBuffer.makeCopyOf (buffer, true);
    monoBuffer.clear();

    const auto inputGain = buffer.getNumChannels() > 0
                               ? 1.0f / static_cast<float> (buffer.getNumChannels())
                               : 1.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        monoBuffer.addFrom (0, 0, buffer, ch, 0, numSamples, inputGain);

    std::array<float, kNumTaps> tapTimes {};
    std::array<float, kNumTaps> tapLevels {};
    std::array<float, kNumTaps> tapPans {};

    for (size_t i = 0; i < kNumTaps; ++i)
    {
        tapTimes[i]  = apvts.getRawParameterValue (kTapTimeIds[i])->load();
        tapLevels[i] = apvts.getRawParameterValue (kTapLevelIds[i])->load();
        tapPans[i]   = apvts.getRawParameterValue (kTapPanIds[i])->load();
    }

    const auto hpf   = apvts.getRawParameterValue (kHpfId)->load();
    const auto lpf   = apvts.getRawParameterValue (kLpfId)->load();
    const auto swing = apvts.getRawParameterValue (kSwingId)->load();
    const auto mix   = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (kMixId)->load());

    wetBuffer.clear();

    juce::dsp::AudioBlock<const float> monoBlock (monoBuffer);
    juce::dsp::AudioBlock<float> tapBlock (tapScratchBuffer);
    auto monoChannelBlock = monoBlock.getSingleChannelBlock (0);
    auto tapChannelBlock  = tapBlock.getSingleChannelBlock (0);

    for (size_t tapIdx = 0; tapIdx < kNumTaps; ++tapIdx)
    {
        tapScratchBuffer.clear();

        auto swingDirection = (tapIdx % 2 == 0 ? -1.0f : 1.0f);
        const auto swingOffset = 1.0f + swingDirection * swing * 0.35f;
        auto timeMs = tapTimes[tapIdx] * swingOffset;
        timeMs = juce::jlimit (10.0f, 2000.0f, timeMs);
        auto delaySamples = timeMs * 0.001f * static_cast<float> (currentSampleRate);
        const auto maxDelay = static_cast<float> (tapDelayLines[tapIdx].getMaximumDelayInSamples() - 2);
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);

        tapDelayLines[tapIdx].setDelay (delaySamples);
        auto tapContext = juce::dsp::ProcessContextNonReplacing<float> (monoChannelBlock, tapChannelBlock);
        tapDelayLines[tapIdx].process (tapContext);

        auto gain = juce::jlimit (0.0f, 1.0f, tapLevels[tapIdx]);
        tapScratchBuffer.applyGain (gain);

        const auto pan   = juce::jlimit (-1.0f, 1.0f, tapPans[tapIdx]);
        const auto angle = juce::jmap (pan, -1.0f, 1.0f,
                                       0.0f,
                                       juce::MathConstants<float>::halfPi);
        const auto leftGain  = std::cos (angle);
        const auto rightGain = std::sin (angle);

        if (wetBuffer.getNumChannels() > 0)
            wetBuffer.addFrom (0, 0, tapScratchBuffer, 0, 0, numSamples, leftGain);

        if (wetBuffer.getNumChannels() > 1)
            wetBuffer.addFrom (1, 0, tapScratchBuffer, 0, 0, numSamples, rightGain);
    }

    updateFilters (hpf, lpf);
    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    auto wetCtx = juce::dsp::ProcessContextReplacing<float> (wetBlock);
    hpfProcessor.process (wetCtx);
    lpfProcessor.process (wetCtx);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
        buffer.applyGain (ch, 0, numSamples, 1.0f - mix);
        buffer.addFrom (ch, 0, wetBuffer, juce::jmin (ch, wetBuffer.getNumChannels() - 1),
                        0, numSamples, mix);
    }
}

//==============================================================================
juce::AudioProcessorEditor* PITTimeStackAudioProcessor::createEditor()
{
    return new PITTimeStackAudioProcessorEditor (*this);
}

void PITTimeStackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void PITTimeStackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout PITTimeStackAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addTapParams = [&params](const std::array<const char*, 4>& ids,
                                  const juce::String& prefix,
                                  const juce::NormalisableRange<float>& range,
                                  float defaultValue)
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                ids[i],
                prefix + juce::String ((int) i + 1),
                range,
                defaultValue));
        }
    };

    addTapParams (kTapTimeIds,  "Tap Time ",  juce::NormalisableRange<float> (10.0f, 2000.0f, 0.1f, 0.4f), 250.0f);
    addTapParams (kTapLevelIds, "Tap Level ", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f);
    addTapParams (kTapPanIds,   "Tap Pan ",   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kHpfId, "HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kLpfId, "LPF",
                                                                   juce::NormalisableRange<float> (2000.0f, 20000.0f, 0.01f, 0.35f), 15000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kSwingId, "Swing",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kMixId, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
PITTimeStackAudioProcessorEditor::PITTimeStackAudioProcessorEditor (PITTimeStackAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    setSize (700, 420);

    for (auto& slider : tapTimeSliders)  initSlider (slider, "Time");
    for (auto& slider : tapLevelSliders) initSlider (slider, "Level");
    for (auto& slider : tapPanSliders)   initSlider (slider, "Pan");
    initSlider (hpfSlider,   "HPF");
    initSlider (lpfSlider,   "LPF");
    initSlider (swingSlider, "Swing");
    initSlider (mixSlider,   "Mix");

    auto& vts = processorRef.getValueTreeState();
    auto addAttachment = [this, &vts](const juce::String& paramId, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (vts, paramId, slider));
    };

    for (size_t i = 0; i < tapTimeSliders.size(); ++i)
    {
        addAttachment (kTapTimeIds[i],   tapTimeSliders[i]);
        addAttachment (kTapLevelIds[i],  tapLevelSliders[i]);
        addAttachment (kTapPanIds[i],    tapPanSliders[i]);
    }

    addAttachment (kHpfId,   hpfSlider);
    addAttachment (kLpfId,   lpfSlider);
    addAttachment (kSwingId, swingSlider);
    addAttachment (kMixId,   mixSlider);
}

void PITTimeStackAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawFittedText ("PIT Time Stack", getLocalBounds().removeFromTop (30), juce::Justification::centred, 1);
}

void PITTimeStackAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (40);

    auto layoutRow = [&area](auto& sliderArray)
    {
        auto row = area.removeFromTop (90);
        auto width = row.getWidth() / (int) sliderArray.size();
        for (auto& slider : sliderArray)
            slider.setBounds (row.removeFromLeft (width).reduced (6));
    };

    layoutRow (tapTimeSliders);
    layoutRow (tapLevelSliders);
    layoutRow (tapPanSliders);

    auto bottomRow = area.removeFromTop (100);
    const int bottomWidth = bottomRow.getWidth() / 4;
    hpfSlider  .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (6));
    lpfSlider  .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (6));
    swingSlider.setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (6));
    mixSlider  .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (6));
}

void PITTimeStackAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setName (labelText);
    addAndMakeVisible (slider);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PITTimeStackAudioProcessor();
}

void PITTimeStackAudioProcessor::updateFilters (float hpf, float lpf)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto hpfCutoff = juce::jlimit (20.0f, 2000.0f, hpf);
    const auto lpfCutoff = juce::jlimit (500.0f, 20000.0f, lpf);

    if (hpfProcessor.state != nullptr)
    {
        *hpfProcessor.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpfCutoff);
        lastHpfCutoff = hpfCutoff;
    }

    if (lpfProcessor.state != nullptr)
    {
        *lpfProcessor.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, lpfCutoff);
        lastLpfCutoff = lpfCutoff;
    }
}

void PITTimeStackAudioProcessor::ensureBuffers (int numChannels, int numSamples)
{
    const auto channelCount = juce::jmax (2, numChannels);
    const auto samples = juce::jmax (1, numSamples);
    dryBuffer.setSize (channelCount, samples, false, false, true);
    wetBuffer.setSize (channelCount, samples, false, false, true);
    monoBuffer.setSize (1, samples, false, false, true);
    tapScratchBuffer.setSize (1, samples, false, false, true);
}
