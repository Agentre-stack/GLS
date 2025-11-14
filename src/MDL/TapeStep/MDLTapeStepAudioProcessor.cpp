#include "MDLTapeStepAudioProcessor.h"

MDLTapeStepAudioProcessor::MDLTapeStepAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TAPE_STEP", createParameterLayout())
{
}

void MDLTapeStepAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
}

void MDLTapeStepAudioProcessor::releaseResources()
{
}

void MDLTapeStepAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto timeMs   = get ("time");
    const auto feedback = juce::jlimit (0.0f, 0.95f, get ("feedback"));
    const auto drive    = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const auto wow      = juce::jlimit (0.0f, 1.0f, get ("wow"));
    const auto flutter  = juce::jlimit (0.0f, 1.0f, get ("flutter"));
    const auto tone     = juce::jlimit (-1.0f, 1.0f, get ("tone"));
    const auto mix      = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const float delaySamples = juce::jlimit (10.0f,
                                             (float) (currentSampleRate * 2.5f),
                                             timeMs * 0.001f * (float) currentSampleRate);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);

    updateToneFilters (tone);

    const float wowRate = juce::jmap (wow, 0.05f, 0.3f);
    const float flutterRate = juce::jmap (flutter, 1.0f, 6.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* dry  = dryBuffer.getReadPointer (ch);
        auto& line = tapeLines[ch];

        line.delay.setDelay (delaySamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dry[i];

            // wow/flutter modulation
            const float wowMod = std::sin (line.wowPhase) * wow * 3.0f;
            const float flutterMod = std::sin (line.flutterPhase) * flutter * 0.8f;
            const float modulatedDelay = delaySamples + wowMod + flutterMod;
            line.delay.setDelay (juce::jlimit (1.0f, (float) (currentSampleRate * 2.5f), modulatedDelay));

            float delayed = line.delay.popSample (0);
            delayed = line.toneFilter.processSample (delayed);

            const float saturation = std::tanh ((delayed + drySample * 0.2f) * (1.0f + drive * 4.0f));
            const float tapeSample = juce::jlimit (-1.0f, 1.0f, saturation);

            const float feedbackInput = drySample + tapeSample * feedback;
            line.delay.pushSample (0, feedbackInput);
            line.feedbackSample = tapeSample;

            data[i] = tapeSample * mix + drySample * (1.0f - mix);

            line.wowPhase += wowRate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            line.flutterPhase += flutterRate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            if (line.wowPhase > juce::MathConstants<float>::twoPi) line.wowPhase -= juce::MathConstants<float>::twoPi;
            if (line.flutterPhase > juce::MathConstants<float>::twoPi) line.flutterPhase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void MDLTapeStepAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLTapeStepAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLTapeStepAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("time",     "Time",
                                                                   juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.4f), 450.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback",
                                                                   juce::NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive",    "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("wow",      "Wow",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("flutter",  "Flutter",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone",     "Tone",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",      "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLTapeStepAudioProcessorEditor::MDLTapeStepAudioProcessorEditor (MDLTapeStepAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (timeSlider,     "Time");
    make (feedbackSlider, "Feedback");
    make (driveSlider,    "Drive");
    make (wowSlider,      "Wow");
    make (flutterSlider,  "Flutter");
    make (toneSlider,     "Tone");
    make (mixSlider,      "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "time", "feedback", "drive", "wow", "flutter", "tone", "mix" };
    juce::Slider* sliders[] = { &timeSlider, &feedbackSlider, &driveSlider, &wowSlider, &flutterSlider, &toneSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (760, 300);
}

void MDLTapeStepAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLTapeStepAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Tape Step", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLTapeStepAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 7;

    timeSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    feedbackSlider.setBounds (area.removeFromLeft (width).reduced (8));
    driveSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    wowSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    flutterSlider .setBounds (area.removeFromLeft (width).reduced (8));
    toneSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider     .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLTapeStepAudioProcessor::createEditor()
{
    return new MDLTapeStepAudioProcessorEditor (*this);
}

void MDLTapeStepAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) tapeLines.size() < numChannels)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        const auto previous = (int) tapeLines.size();
        tapeLines.resize (numChannels);
        for (int ch = previous; ch < numChannels; ++ch)
        {
            tapeLines[ch].delay.setMaximumDelayInSamples ((int) (currentSampleRate * 3.0f));
            tapeLines[ch].delay.prepare (spec);
            tapeLines[ch].delay.reset();
            tapeLines[ch].toneFilter.prepare (spec);
            tapeLines[ch].toneFilter.reset();
        }
    }
    else
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        for (auto& line : tapeLines)
        {
            line.delay.prepare (spec);
            line.toneFilter.prepare (spec);
        }
    }
}

void MDLTapeStepAudioProcessor::updateToneFilters (float tone)
{
    if (currentSampleRate <= 0.0)
        return;

    const float baseFreq = juce::jmap (tone, -1.0f, 1.0f, 800.0f, 6000.0f);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, baseFreq, 0.8f);

    for (auto& line : tapeLines)
        line.toneFilter.coefficients = coeffs;
}
