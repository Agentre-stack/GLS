#include "GLSSubCommandAudioProcessor.h"

GLSSubCommandAudioProcessor::GLSSubCommandAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUB_COMMAND", createParameterLayout())
{
}

void GLSSubCommandAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    for (auto& state : channelStates)
    {
        state.lowPass.reset();
        state.outputHPF.reset();
        state.envelope = 0.0f;
        state.gain = 1.0f;
    }
}

void GLSSubCommandAudioProcessor::releaseResources()
{
}

void GLSSubCommandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto xoverFreq = read ("xover_freq");
    const auto subLevelDb= read ("sub_level");
    const auto tightness = juce::jlimit (0.0f, 1.0f, read ("tightness"));
    const auto harmonics = juce::jlimit (0.0f, 1.0f, read ("harmonics"));
    const auto outHpf    = read ("out_hpf");
    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));

    ensureStateSize();
    originalBuffer.makeCopyOf (buffer, true);

    const auto subGain     = juce::Decibels::decibelsToGain (subLevelDb);
    const auto attackCoeff = std::exp (-1.0f / ((0.5f + tightness * 4.5f) * 0.001f * currentSampleRate));
    const auto releaseCoeff= std::exp (-1.0f / ((10.0f - tightness * 9.0f) * 0.001f * currentSampleRate));

    const int numSamples   = buffer.getNumSamples();
    const int numChannels  = buffer.getNumChannels();

    juce::AudioBuffer<float> lowBuffer (numChannels, numSamples);
    juce::AudioBuffer<float> highBuffer (numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        updateFilters (state, xoverFreq, outHpf);

        const auto* input = originalBuffer.getReadPointer (ch);
        auto* lowPtr  = lowBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            lowPtr[i] = state.lowPass.processSample (input[i]);

        auto* highPtr = highBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            highPtr[i] = input[i] - lowPtr[i];

        for (int i = 0; i < numSamples; ++i)
        {
            auto& env = state.envelope;
            const auto level = std::abs (lowPtr[i]);
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const auto targetGain = env > 0.0f ? juce::jmap (tightness, 1.0f, 0.5f) : 1.0f;
            state.gain += 0.02f * (targetGain - state.gain);

            float sample = lowPtr[i] * state.gain;
            sample = generateHarmonics (sample, harmonics);
            sample *= subGain;
            lowPtr[i] = sample;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* lowPtr  = lowBuffer.getWritePointer (ch);
        auto* highPtr = highBuffer.getWritePointer (ch);
        auto* outPtr  = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float processed = lowPtr[i] + highPtr[i];
            const float original  = originalBuffer.getSample (ch, i);
            auto sample = processed * mix + original * (1.0f - mix);
            outPtr[i] = state.outputHPF.processSample (sample);
        }
    }
}

void GLSSubCommandAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSSubCommandAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSSubCommandAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("xover_freq", "Xover Freq",
                                                                   juce::NormalisableRange<float> (40.0f, 250.0f, 0.01f, 0.4f), 90.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_level",  "Sub Level",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tightness",  "Tightness",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("harmonics",  "Harmonics",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("out_hpf",    "Out HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 120.0f, 0.01f, 0.4f), 35.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

GLSSubCommandAudioProcessorEditor::GLSSubCommandAudioProcessorEditor (GLSSubCommandAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& s, const juce::String& label) { initialiseSlider (s, label); };

    makeSlider (xoverFreqSlider, "Xover");
    makeSlider (subLevelSlider,  "Sub");
    makeSlider (tightnessSlider, "Tight");
    makeSlider (harmonicsSlider, "Harm");
    makeSlider (outHpfSlider,    "Out HPF");
    makeSlider (mixSlider,       "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "xover_freq", "sub_level", "tightness", "harmonics", "out_hpf", "mix" };
    juce::Slider* sliders[]      = { &xoverFreqSlider, &subLevelSlider, &tightnessSlider, &harmonicsSlider, &outHpfSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (600, 260);
}

void GLSSubCommandAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSSubCommandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Sub Command", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSSubCommandAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto width = area.getWidth() / 3;

    auto topRow = area.removeFromTop (area.getHeight() / 2);
    xoverFreqSlider.setBounds (topRow.removeFromLeft (width).reduced (8));
    subLevelSlider .setBounds (topRow.removeFromLeft (width).reduced (8));
    tightnessSlider.setBounds (topRow.removeFromLeft (width).reduced (8));

    harmonicsSlider.setBounds (area.removeFromLeft (width).reduced (8));
    outHpfSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider      .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GLSSubCommandAudioProcessor::createEditor()
{
    return new GLSSubCommandAudioProcessorEditor (*this);
}

void GLSSubCommandAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}

void GLSSubCommandAudioProcessor::updateFilters (ChannelState& state, float xoverFreq, float outHpfFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (20.0f, (float) (currentSampleRate * 0.45), xoverFreq));
    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                       juce::jlimit (10.0f, (float) (currentSampleRate * 0.45), outHpfFreq));

    state.lowPass.coefficients   = lp;
    state.outputHPF.coefficients = hp;
}

float GLSSubCommandAudioProcessor::generateHarmonics (float sample, float amount)
{
    if (amount <= 0.0f)
        return sample;

    const float second = sample * sample * (sample >= 0.0f ? 1.0f : -1.0f);
    const float saturated = std::tanh (sample * juce::jmap (amount, 1.0f, 5.0f));
    return juce::jmap (amount, sample, 0.5f * (saturated + second));
}
