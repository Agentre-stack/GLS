#include "GLSChannelStripOneAudioProcessor.h"

GLSChannelStripOneAudioProcessor::GLSChannelStripOneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHANNEL_STRIP_ONE", createParameterLayout())
{
}

void GLSChannelStripOneAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();

    for (auto& state : channelStates)
    {
        state.lowShelf.reset();
        state.lowMidBell.reset();
        state.highMidBell.reset();
        state.highShelf.reset();
        state.gateEnvelope = 0.0f;
        state.gateGain = 1.0f;
        state.compEnvelope = 0.0f;
        state.compGain = 1.0f;
    }
}

void GLSChannelStripOneAudioProcessor::releaseResources()
{
}

void GLSChannelStripOneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto readParam = [this](const juce::String& id)
    {
        return apvts.getRawParameterValue (id)->load();
    };

    const auto gateThresh  = readParam ("gate_thresh");
    const auto gateRange   = readParam ("gate_range");
    const auto compThresh  = readParam ("comp_thresh");
    const auto compRatio   = juce::jmax (1.0f, readParam ("comp_ratio"));
    const auto compAttack  = juce::jmax (0.1f, readParam ("comp_attack"));
    const auto compRelease = juce::jmax (1.0f, readParam ("comp_release"));
    const auto lowGain     = readParam ("low_gain");
    const auto lowMidGain  = readParam ("low_mid_gain");
    const auto highMidGain = readParam ("high_mid_gain");
    const auto highGain    = readParam ("high_gain");
    const auto satAmount   = juce::jlimit (0.0f, 1.0f, readParam ("sat_amount"));
    const auto mix         = juce::jlimit (0.0f, 1.0f, readParam ("mix"));

    ensureStateSize();
    dryBuffer.makeCopyOf (buffer, true);

    const auto gateThresholdLinear = juce::Decibels::decibelsToGain (gateThresh);
    const auto gateAttenuation     = juce::Decibels::decibelsToGain (-juce::jmax (0.0f, gateRange));
    const auto compThresholdDb     = compThresh;
    const auto attackCoeff         = std::exp (-1.0f / ((compAttack * 0.001f) * currentSampleRate));
    const auto releaseCoeff        = std::exp (-1.0f / ((compRelease * 0.001f) * currentSampleRate));
    const auto gateEnvCoeff        = std::exp (-1.0f / (0.005f * currentSampleRate));

    const auto numSamples = buffer.getNumSamples();
    const auto totalChannels = buffer.getNumChannels();

    for (int ch = 0; ch < totalChannels; ++ch)
    {
        auto& state = channelStates[ch];
        updateEqCoefficients (state, lowGain, lowMidGain, highMidGain, highGain);

        auto* data = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];

            // Gate
            const auto level = std::abs (sample);
            state.gateEnvelope = gateEnvCoeff * state.gateEnvelope + (1.0f - gateEnvCoeff) * level;
            const auto targetGateGain = state.gateEnvelope >= gateThresholdLinear ? 1.0f : gateAttenuation;
            state.gateGain += 0.002f * (targetGateGain - state.gateGain);
            sample *= state.gateGain;

            // Compressor
            const auto detector = std::abs (sample);
            auto& env = state.compEnvelope;
            if (detector > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * detector;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * detector;

            auto envDb = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f));
            float gainDb = 0.0f;
            if (envDb > compThresholdDb)
            {
                const auto over = envDb - compThresholdDb;
                const auto compressed = compThresholdDb + over / compRatio;
                gainDb = compressed - envDb;
            }

            const auto targetCompGain = juce::Decibels::decibelsToGain (gainDb);
            state.compGain += 0.01f * (targetCompGain - state.compGain);
            sample *= state.compGain;

            // 4-band EQ
            sample = state.lowShelf.processSample (sample);
            sample = state.lowMidBell.processSample (sample);
            sample = state.highMidBell.processSample (sample);
            sample = state.highShelf.processSample (sample);

            // Saturation
            sample = softClip (sample, satAmount);

            data[i] = sample;
        }

        // Mix
        for (int i = 0; i < numSamples; ++i)
            data[i] = data[i] * mix + dry[i] * (1.0f - mix);
    }

    // TODO: Saturation (EQ -> Saturation)
}

void GLSChannelStripOneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSChannelStripOneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData (data, sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSChannelStripOneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange   = juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f);
    auto gainRange = juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f);
    auto timeRange = juce::NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.25f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gate_thresh",  "Gate Thresh", dBRange, -40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gate_range",   "Gate Range",  juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_thresh",  "Comp Thresh", dBRange, -20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_ratio",   "Comp Ratio",  juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_attack",  "Comp Attack", timeRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("comp_release", "Comp Release", juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f), 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_gain",     "Low Gain",     gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_mid_gain", "LowMid Gain",  gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_mid_gain","HighMid Gain", gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_gain",    "High Gain",    gainRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sat_amount",   "Sat Amount",   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",          juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

GLSChannelStripOneAudioProcessorEditor::GLSChannelStripOneAudioProcessorEditor (GLSChannelStripOneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& slider, const juce::String& label)
    {
        initialiseSlider (slider, label);
    };

    makeSlider (gateThreshSlider,  "Gate Thresh");
    makeSlider (gateRangeSlider,   "Gate Range");
    makeSlider (compThreshSlider,  "Comp Thresh");
    makeSlider (compRatioSlider,   "Comp Ratio");
    makeSlider (compAttackSlider,  "Attack");
    makeSlider (compReleaseSlider, "Release");
    makeSlider (lowGainSlider,     "Low");
    makeSlider (lowMidGainSlider,  "LowMid");
    makeSlider (highMidGainSlider, "HighMid");
    makeSlider (highGainSlider,    "High");
    makeSlider (satAmountSlider,   "Sat");
    makeSlider (mixSlider,         "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "gate_thresh", "gate_range", "comp_thresh", "comp_ratio",
        "comp_attack", "comp_release", "low_gain", "low_mid_gain",
        "high_mid_gain", "high_gain", "sat_amount", "mix"
    };

    juce::Slider* sliders[] = {
        &gateThreshSlider, &gateRangeSlider, &compThreshSlider, &compRatioSlider,
        &compAttackSlider, &compReleaseSlider, &lowGainSlider, &lowMidGainSlider,
        &highMidGainSlider, &highGainSlider, &satAmountSlider, &mixSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 360);
}

void GLSChannelStripOneAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSChannelStripOneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Channel Strip One", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSChannelStripOneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto rowHeight = 160;

    auto topRow = area.removeFromTop (rowHeight);
    auto bottomRow = area.removeFromTop (rowHeight);

    auto placeRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto segment = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (segment).reduced (10));
    };

    placeRow (topRow,     { &gateThreshSlider, &gateRangeSlider, &compThreshSlider, &compRatioSlider, &compAttackSlider, &compReleaseSlider });
    placeRow (bottomRow,  { &lowGainSlider, &lowMidGainSlider, &highMidGainSlider, &highGainSlider, &satAmountSlider, &mixSlider });
}

juce::AudioProcessorEditor* GLSChannelStripOneAudioProcessor::createEditor()
{
    return new GLSChannelStripOneAudioProcessorEditor (*this);
}

void GLSChannelStripOneAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}

void GLSChannelStripOneAudioProcessor::updateEqCoefficients (ChannelState& state,
                                                             float lowGain, float lowMidGain,
                                                             float highMidGain, float highGain)
{
    const auto lowShelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 120.0f, 0.707f,
                                                                                   juce::Decibels::decibelsToGain (lowGain));
    const auto lowMidCoeffs   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 400.0f, 0.9f,
                                                                                     juce::Decibels::decibelsToGain (lowMidGain));
    const auto highMidCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 3000.0f, 0.9f,
                                                                                     juce::Decibels::decibelsToGain (highMidGain));
    const auto highShelfCoeffs= juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 8000.0f, 0.707f,
                                                                                    juce::Decibels::decibelsToGain (highGain));

    state.lowShelf.coefficients  = lowShelfCoeffs;
    state.lowMidBell.coefficients= lowMidCoeffs;
    state.highMidBell.coefficients= highMidCoeffs;
    state.highShelf.coefficients = highShelfCoeffs;
}

float GLSChannelStripOneAudioProcessor::softClip (float input, float amount)
{
    if (amount <= 0.0f)
        return input;

    const auto drive = juce::jmap (amount, 1.0f, 6.0f);
    const auto saturated = std::tanh (input * drive);
    return juce::jmap (amount, input, saturated);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSChannelStripOneAudioProcessor();
}
