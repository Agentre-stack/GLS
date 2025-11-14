#include "AEVAmbienceEvolverSuiteAudioProcessor.h"

AEVAmbienceEvolverSuiteAudioProcessor::AEVAmbienceEvolverSuiteAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AMBIENCE_EVOLVER", createParameterLayout())
{
}

void AEVAmbienceEvolverSuiteAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
}

void AEVAmbienceEvolverSuiteAudioProcessor::releaseResources()
{
}

void AEVAmbienceEvolverSuiteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto ambienceLevel = juce::jlimit (0.0f, 1.0f, get ("ambience_level"));
    const auto deVerb        = juce::jlimit (0.0f, 1.0f, get ("deverb"));
    const auto noiseSupp     = juce::jlimit (0.0f, 1.0f, get ("noise_suppression"));
    const auto transientProt = juce::jlimit (0.0f, 1.0f, get ("transient_protect"));
    const auto toneMatch     = juce::jlimit (0.0f, 1.0f, get ("tone_match"));
    const auto hfRecover     = juce::jlimit (0.0f, 1.0f, get ("hf_recover"));
    const auto outputTrimDb  = get ("output_trim");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    ensureStateSize (numChannels);

    const float ambienceBlend = ambienceLevel * 0.8f;
    const float deVerbDecay = juce::jmap (deVerb, 0.1f, 0.9f);
    const float toneBlend = toneMatch * 0.5f;
    const float hfGain = juce::Decibels::decibelsToGain (hfRecover * 6.0f);
    const float outputGain = juce::Decibels::decibelsToGain (outputTrimDb);
    const float transientThresh = juce::Decibels::decibelsToGain (-20.0f + transientProt * 10.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelStates[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            const float absSample = std::abs (sample);

            // Noise profiling / ambience estimation
            state.noiseEstimate = 0.999f * state.noiseEstimate + 0.001f * absSample;
            updateProfileState (absSample);

            const float ambience = state.ambienceState = 0.995f * state.ambienceState + 0.005f * sample;
            const float ambienceRemoved = sample - ambience * ambienceBlend;

            const float smear = state.toneState = state.toneState + deVerbDecay * (ambienceRemoved - state.toneState);
            float cleaned = ambienceRemoved - smear * deVerb;

            const float noiseFloor = juce::jmax (state.noiseEstimate, capturedNoise + 1.0e-6f);
            const float gate = juce::jlimit (0.0f, 1.0f, (absSample - noiseFloor) / (noiseFloor + 1.0e-6f));
            const float noiseReduction = juce::jmap (noiseSupp, 0.0f, 1.0f, gate, gate * 0.2f);
            cleaned *= noiseReduction;

            const float transientEnv = state.transientState = juce::jmax (absSample, state.transientState * 0.97f);
            if (transientEnv > transientThresh)
                cleaned = juce::jlimit (-std::abs(sample), std::abs(sample), cleaned + (sample - cleaned) * transientProt * 0.6f);

            const float toneTarget = sample * 0.5f;
            cleaned = cleaned * (1.0f - toneBlend) + toneTarget * toneBlend;

            const float hfSignal = cleaned - (state.toneState = 0.98f * state.toneState + 0.02f * cleaned);
            cleaned += hfSignal * (hfGain - 1.0f);

            data[i] = cleaned * outputGain;
        }
    }
}

void AEVAmbienceEvolverSuiteAudioProcessor::triggerProfileCapture()
{
    profileCaptureArmed = true;
    profileSamplesRemaining = (int) currentSampleRate / 2;
    profileAccumulator = 0.0f;
    profileTotalSamples = profileSamplesRemaining;
}

void AEVAmbienceEvolverSuiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void AEVAmbienceEvolverSuiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
AEVAmbienceEvolverSuiteAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ambience_level", "Ambience Level",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deverb",          "De-Verb",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("noise_suppression","Noise",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("transient_protect","Transient",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone_match",      "Tone Match",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hf_recover",      "HF Recover",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",     "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

AEVAmbienceEvolverSuiteAudioProcessorEditor::AEVAmbienceEvolverSuiteAudioProcessorEditor (AEVAmbienceEvolverSuiteAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (profileButton);
    profileButton.addListener (this);

    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (ambienceSlider, "Ambience");
    make (deVerbSlider,   "De-Verb");
    make (noiseSlider,    "Noise");
    make (transientSlider,"Transient");
    make (toneMatchSlider,"Tone Match");
    make (hfRecoverSlider,"HF Recover");
    make (outputSlider,   "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "ambience_level", "deverb", "noise_suppression", "transient_protect",
        "tone_match", "hf_recover", "output_trim"
    };
    juce::Slider* sliders[] = {
        &ambienceSlider, &deVerbSlider, &noiseSlider,
        &transientSlider, &toneMatchSlider, &hfRecoverSlider, &outputSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (840, 300);
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &profileButton)
        processorRef.triggerProfileCapture();
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("AEV Ambience Evolver", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void AEVAmbienceEvolverSuiteAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    profileButton.setBounds (area.removeFromTop (32));

    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 7;

    ambienceSlider .setBounds (row.removeFromLeft (width).reduced (8));
    deVerbSlider   .setBounds (row.removeFromLeft (width).reduced (8));
    noiseSlider    .setBounds (row.removeFromLeft (width).reduced (8));
    transientSlider.setBounds (row.removeFromLeft (width).reduced (8));
    toneMatchSlider.setBounds (row.removeFromLeft (width).reduced (8));
    hfRecoverSlider.setBounds (row.removeFromLeft (width).reduced (8));
    outputSlider   .setBounds (row.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* AEVAmbienceEvolverSuiteAudioProcessor::createEditor()
{
    return new AEVAmbienceEvolverSuiteAudioProcessorEditor (*this);
}

void AEVAmbienceEvolverSuiteAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) channelStates.size() < numChannels)
        channelStates.resize (numChannels);
}

void AEVAmbienceEvolverSuiteAudioProcessor::updateProfileState (float sampleEnv)
{
    if (! profileCaptureArmed)
        return;

    profileAccumulator += sampleEnv;

    if (--profileSamplesRemaining <= 0)
    {
        profileCaptureArmed = false;
        capturedNoise = profileAccumulator / (float) juce::jmax (1, profileTotalSamples);
    }
}
