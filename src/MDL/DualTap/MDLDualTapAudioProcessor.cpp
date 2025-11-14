#include "MDLDualTapAudioProcessor.h"

MDLDualTapAudioProcessor::MDLDualTapAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DUAL_TAP", createParameterLayout())
{
}

void MDLDualTapAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    auto prepareTap = [&](auto& taps)
    {
        for (auto& tap : taps)
        {
            tap.delay.setMaximumDelayInSamples ((int) (currentSampleRate * 2.5));
            tap.delay.prepare (spec);
            tap.delay.reset();
            tap.hpf.prepare (spec);
            tap.hpf.reset();
            tap.lpf.prepare (spec);
            tap.lpf.reset();
        }
    };

    prepareTap (tapA);
    prepareTap (tapB);
}

void MDLDualTapAudioProcessor::releaseResources()
{
}

void MDLDualTapAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto timeAms = get ("time_a");
    const auto timeBms = get ("time_b");
    const auto feedback = juce::jlimit (0.0f, 0.95f, get ("feedback"));
    const auto panA = juce::jlimit (-1.0f, 1.0f, get ("pan_a"));
    const auto panB = juce::jlimit (-1.0f, 1.0f, get ("pan_b"));
    const auto hpf = get ("hpf");
    const auto lpf = get ("lpf");
    const auto mix = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const float delaySamplesA = juce::jlimit (1.0f, (float) (currentSampleRate * 2.0f), timeAms * 0.001f * (float) currentSampleRate);
    const float delaySamplesB = juce::jlimit (1.0f, (float) (currentSampleRate * 2.0f), timeBms * 0.001f * (float) currentSampleRate);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);

    updateFilters (hpf, lpf);

    auto calcPan = [](float pan)
    {
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        return std::pair<float, float> (std::cos (angle), std::sin (angle));
    };

    const auto panGainsA = calcPan (panA);
    const auto panGainsB = calcPan (panB);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer (ch);
        auto* dryData = dryBuffer.getReadPointer (ch);
        auto& tapStateA = tapA[ch];
        auto& tapStateB = tapB[ch];

        tapStateA.delay.setDelay (delaySamplesA);
        tapStateB.delay.setDelay (delaySamplesB);

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dryData[i];

            float delayedA = tapStateA.delay.popSample (0);
            float delayedB = tapStateB.delay.popSample (0);

            delayedA = tapStateA.hpf.processSample (0, delayedA);
            delayedA = tapStateA.lpf.processSample (0, delayedA);

            delayedB = tapStateB.hpf.processSample (0, delayedB);
            delayedB = tapStateB.lpf.processSample (0, delayedB);

            tapStateA.delay.pushSample (0, drySample + delayedA * feedback);
            tapStateB.delay.pushSample (0, drySample + delayedB * feedback);

            float tapOut = 0.0f;
            const bool isLeft = (ch % 2) == 0;
            if (isLeft)
                tapOut = delayedA * panGainsA.first + delayedB * panGainsB.first;
            else
                tapOut = delayedA * panGainsA.second + delayedB * panGainsB.second;

            wetData[i] = drySample * (1.0f - mix) + tapOut * mix;
        }
    }
}

void MDLDualTapAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLDualTapAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLDualTapAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("time_a",   "Time A",
                                                                   juce::NormalisableRange<float> (10.0f, 2000.0f, 0.01f, 0.4f), 350.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("time_b",   "Time B",
                                                                   juce::NormalisableRange<float> (10.0f, 2000.0f, 0.01f, 0.4f), 500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback",
                                                                   juce::NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pan_a",   "Pan A",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), -0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pan_b",   "Pan B",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf",     "HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lpf",     "LPF",
                                                                   juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.01f, 0.35f), 8000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",     "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLDualTapAudioProcessorEditor::MDLDualTapAudioProcessorEditor (MDLDualTapAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (timeASlider,    "Time A");
    make (timeBSlider,    "Time B");
    make (feedbackSlider, "Feedback");
    make (panASlider,     "Pan A");
    make (panBSlider,     "Pan B");
    make (hpfSlider,      "HPF");
    make (lpfSlider,      "LPF");
    make (mixSlider,      "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "time_a", "time_b", "feedback", "pan_a", "pan_b", "hpf", "lpf", "mix" };
    juce::Slider* sliders[]      = { &timeASlider, &timeBSlider, &feedbackSlider, &panASlider, &panBSlider, &hpfSlider, &lpfSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (760, 320);
}

void MDLDualTapAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void MDLDualTapAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Dual Tap", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLDualTapAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (top,  { &timeASlider, &timeBSlider, &feedbackSlider, &panASlider, &panBSlider });
    layoutRow (area, { &hpfSlider, &lpfSlider, &mixSlider });
}

juce::AudioProcessorEditor* MDLDualTapAudioProcessor::createEditor()
{
    return new MDLDualTapAudioProcessorEditor (*this);
}

void MDLDualTapAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) tapA.size() < numChannels)
        tapA.resize (numChannels);
    if ((int) tapB.size() < numChannels)
        tapB.resize (numChannels);
}

void MDLDualTapAudioProcessor::updateFilters (float hpf, float lpf)
{
    if (currentSampleRate <= 0.0)
        return;

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                       juce::jlimit (20.0f, 5000.0f, hpf),
                                                                       0.707f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (1000.0f, (float) (currentSampleRate * 0.49f), lpf),
                                                                      0.707f);
    for (auto& tap : tapA)
    {
        tap.hpf.coefficients = hpCoeffs;
        tap.lpf.coefficients = lpCoeffs;
    }
    for (auto& tap : tapB)
    {
        tap.hpf.coefficients = hpCoeffs;
        tap.lpf.coefficients = lpCoeffs;
    }
}
