#include "MDLGhostEchoAudioProcessor.h"

MDLGhostEchoAudioProcessor::MDLGhostEchoAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GHOST_ECHO", createParameterLayout())
{
}

void MDLGhostEchoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());
    dryBuffer.setSize (channels, (int) lastBlockSize);
    ensureStateSize (channels);
}

void MDLGhostEchoAudioProcessor::releaseResources()
{
}

void MDLGhostEchoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto timeMs  = get ("time");
    const auto feedback = juce::jlimit (0.0f, 0.95f, get ("feedback"));
    const auto blur     = juce::jlimit (0.0f, 1.0f, get ("blur"));
    const auto damping  = juce::jlimit (0.0f, 1.0f, get ("damping"));
    const auto width    = juce::jlimit (0.0f, 2.0f, get ("width"));
    const auto mix      = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    setTapDelayTimes (timeMs);
    updateTapFilters (damping);

    juce::Random random;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        auto& tap = taps[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dry[i];
            float delayed = tap.delay.popSample (0);
            delayed = tap.dampingFilter.processSample (delayed);

            const float blurNoise = (random.nextFloat() * 2.0f - 1.0f) * blur * 0.02f;
            delayed = juce::jlimit (-1.0f, 1.0f, delayed + blurNoise);

            const float feedbackInput = drySample + delayed * feedback;
            tap.delay.pushSample (0, feedbackInput);

            wet[i] = delayed * mix + drySample * (1.0f - mix);
        }
    }

    if (numChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (left[i] + right[i]);
            const float side = 0.5f * (left[i] - right[i]) * width;
            left[i]  = mid + side;
            right[i] = mid - side;
        }
    }
}

void MDLGhostEchoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLGhostEchoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLGhostEchoAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("time",     "Time",
                                                                   juce::NormalisableRange<float> (40.0f, 2000.0f, 0.01f, 0.4f), 480.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback",
                                                                   juce::NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("blur",     "Blur",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("damping",  "Damping",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",    "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",      "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLGhostEchoAudioProcessorEditor::MDLGhostEchoAudioProcessorEditor (MDLGhostEchoAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (timeSlider,     "Time");
    make (feedbackSlider, "Feedback");
    make (blurSlider,     "Blur");
    make (dampingSlider,  "Damping");
    make (widthSlider,    "Width");
    make (mixSlider,      "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "time", "feedback", "blur", "damping", "width", "mix" };
    juce::Slider* sliders[] = { &timeSlider, &feedbackSlider, &blurSlider, &dampingSlider, &widthSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 260);
}

void MDLGhostEchoAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void MDLGhostEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Ghost Echo", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLGhostEchoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 6;

    timeSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    feedbackSlider.setBounds (area.removeFromLeft (width).reduced (8));
    blurSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    dampingSlider .setBounds (area.removeFromLeft (width).reduced (8));
    widthSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider     .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLGhostEchoAudioProcessor::createEditor()
{
    return new MDLGhostEchoAudioProcessorEditor (*this);
}

void MDLGhostEchoAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) taps.size() < numChannels)
    {
        const auto previous = (int) taps.size();
        taps.resize (numChannels);
        for (int ch = previous; ch < numChannels; ++ch)
            taps[ch].feedback = 0.4f;
    }

    const auto targetBlockSize = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (tapSpecSampleRate, currentSampleRate)
                             || tapSpecBlockSize != targetBlockSize;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      targetBlockSize,
                                      1 };
        for (auto& tap : taps)
        {
            tap.delay.setMaximumDelayInSamples ((int) (currentSampleRate * 4.5f));
            tap.delay.prepare (spec);
            tap.delay.reset();
            tap.dampingFilter.prepare (spec);
            tap.dampingFilter.reset();
        }

        tapSpecSampleRate = currentSampleRate;
        tapSpecBlockSize  = targetBlockSize;
    }
}

void MDLGhostEchoAudioProcessor::setTapDelayTimes (float baseTimeMs)
{
    if (currentSampleRate <= 0.0)
        return;

    const float baseSamples = juce::jlimit (10.0f, (float) (currentSampleRate * 4.0f), baseTimeMs * 0.001f * (float) currentSampleRate);
    for (size_t ch = 0; ch < taps.size(); ++ch)
    {
        auto& tap = taps[ch];
        const float scatter = 1.0f + 0.05f * (float) ch;
        tap.delay.setDelay (baseSamples * scatter);
    }
}

void MDLGhostEchoAudioProcessor::updateTapFilters (float damping)
{
    if (currentSampleRate <= 0.0)
        return;

    const float freq = juce::jmap (damping, 0.0f, 1.0f, 18000.0f, 2000.0f);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                    juce::jlimit (2000.0f, (float) (currentSampleRate * 0.45f), freq),
                                                                    0.7f);
    for (auto& tap : taps)
        tap.dampingFilter.coefficients = coeffs;
}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDLGhostEchoAudioProcessor();
}
