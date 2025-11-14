#include "GLSParallelPressAudioProcessor.h"

GLSParallelPressAudioProcessor::GLSParallelPressAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARALLEL_PRESS", createParameterLayout())
{
}

void GLSParallelPressAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    for (auto& state : channelStates)
    {
        state.hpf.reset();
        state.lpf.reset();
        state.envelope = 0.0f;
        state.gain = 1.0f;
    }
}

void GLSParallelPressAudioProcessor::releaseResources()
{
}

void GLSParallelPressAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto drive     = read ("drive");
    const auto thresh    = read ("comp_thresh");
    const auto ratio     = read ("comp_ratio");
    const auto attack    = read ("attack");
    const auto release   = read ("release");
    const auto hpfWet    = read ("hpf_to_wet");
    const auto lpfWet    = read ("lpf_to_wet");
    const auto wetLevel  = read ("wet_level");
    const auto dryLevel  = read ("dry_level");

    ensureStateSize();
    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);

    const auto attackCoeff  = std::exp (-1.0f / (attack * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (release * 0.001f * currentSampleRate));
    const auto threshDb     = thresh;
    const auto wetGain      = juce::Decibels::decibelsToGain (wetLevel);
    const auto dryGain      = juce::Decibels::decibelsToGain (dryLevel);

    const auto numSamples = wetBuffer.getNumSamples();
    const auto numChannels = wetBuffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        updateFilterCoefficients (state, hpfWet, lpfWet);

        auto* wetData = wetBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = wetData[i];
            sample = state.hpf.processSample (sample);
            sample = state.lpf.processSample (sample);

            const auto level = std::abs (sample);
            auto& env = state.envelope;
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const auto levelDb = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f));
            const auto gainDb = computeCompressorGain (levelDb, threshDb, ratio);
            const auto targetGain = juce::Decibels::decibelsToGain (gainDb);
            state.gain += 0.02f * (targetGain - state.gain);

            sample = sample * state.gain;
            sample = applyDrive (sample, drive);

            wetData[i] = sample;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = wetBuffer.getWritePointer (ch);
        auto* dryData = dryBuffer.getWritePointer (ch);
        auto* outData = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            outData[i] = wetData[i] * wetGain + dryData[i] * dryGain;
    }
}

void GLSParallelPressAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSParallelPressAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSParallelPressAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange     = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto timeRange   = juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.3f);
    auto releaseRange= juce::NormalisableRange<float> (10.0f, 500.0f, 0.01f, 0.3f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive",        "Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_thresh",  "Comp Thresh", dBRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_ratio",   "Comp Ratio", juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",       "Attack", timeRange, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",      "Release", releaseRange, 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf_to_wet",   "HPF to Wet", juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lpf_to_wet",   "LPF to Wet", juce::NormalisableRange<float> (2000.0f, 20000.0f, 0.01f, 0.35f), 15000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("wet_level",    "Wet Level", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("dry_level",    "Dry Level", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

GLSParallelPressAudioProcessorEditor::GLSParallelPressAudioProcessorEditor (GLSParallelPressAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& s, const juce::String& label) { initialiseSlider (s, label); };

    makeSlider (driveSlider,      "Drive");
    makeSlider (compThreshSlider, "Comp Thresh");
    makeSlider (compRatioSlider,  "Comp Ratio");
    makeSlider (attackSlider,     "Attack");
    makeSlider (releaseSlider,    "Release");
    makeSlider (hpfWetSlider,     "HPF Wet");
    makeSlider (lpfWetSlider,     "LPF Wet");
    makeSlider (wetLevelSlider,   "Wet Level");
    makeSlider (dryLevelSlider,   "Dry Level");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "drive", "comp_thresh", "comp_ratio", "attack", "release",
        "hpf_to_wet", "lpf_to_wet", "wet_level", "dry_level"
    };

    juce::Slider* sliders[] = {
        &driveSlider, &compThreshSlider, &compRatioSlider, &attackSlider, &releaseSlider,
        &hpfWetSlider, &lpfWetSlider, &wetLevelSlider, &dryLevelSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (760, 320);
}

void GLSParallelPressAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSParallelPressAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Parallel Press", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSParallelPressAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (top,  { &driveSlider, &compThreshSlider, &compRatioSlider, &attackSlider, &releaseSlider });
    layoutRow (area, { &hpfWetSlider, &lpfWetSlider, &wetLevelSlider, &dryLevelSlider });
}

juce::AudioProcessorEditor* GLSParallelPressAudioProcessor::createEditor()
{
    return new GLSParallelPressAudioProcessorEditor (*this);
}

void GLSParallelPressAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}

void GLSParallelPressAudioProcessor::updateFilterCoefficients (ChannelState& state, float hpfFreq, float lpfFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                        juce::jlimit (10.0f, (float) (currentSampleRate * 0.45), hpfFreq));
    const auto lpf = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                       juce::jlimit (100.0f, (float) (currentSampleRate * 0.49), lpfFreq));

    state.hpf.coefficients = hpf;
    state.lpf.coefficients = lpf;
}

float GLSParallelPressAudioProcessor::computeCompressorGain (float levelDb, float threshDb, float ratio) const
{
    if (levelDb <= threshDb)
        return 0.0f;

    const auto over = levelDb - threshDb;
    const auto reduced = over / ratio;
    return threshDb + reduced - levelDb;
}

float GLSParallelPressAudioProcessor::applyDrive (float sample, float drive)
{
    if (drive <= 0.0f)
        return sample;

    const auto driveAmount = juce::jmap (drive, 1.0f, 8.0f);
    const auto saturated = std::tanh (sample * driveAmount);
    return juce::jmap (drive, sample, saturated);
}
