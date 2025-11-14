#include "PITDoubleStrikeAudioProcessor.h"

namespace
{
constexpr auto paramVoiceA = "voice_a_pitch";
constexpr auto paramVoiceB = "voice_b_pitch";
constexpr auto paramDetune = "detune";
constexpr auto paramSpread = "spread";
constexpr auto paramHPF    = "hpf";
constexpr auto paramLPF    = "lpf";
constexpr auto paramMix    = "mix";
}

PITDoubleStrikeAudioProcessor::PITDoubleStrikeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PIT_DOUBLE_STRIKE", createParameterLayout())
{
}

void PITDoubleStrikeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);
    hpfFilters.clear();
    lpfFilters.clear();
}

void PITDoubleStrikeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureState (buffer.getNumChannels(), buffer.getNumSamples());
    dryBuffer.makeCopyOf (buffer, true);

    const auto hpf   = apvts.getRawParameterValue (paramHPF)->load();
    const auto lpf   = apvts.getRawParameterValue (paramLPF)->load();
    const auto spread= apvts.getRawParameterValue (paramSpread)->load();
    const auto mix   = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramMix)->load());

    juce::ignoreUnused (apvts.getRawParameterValue (paramVoiceA)->load());
    juce::ignoreUnused (apvts.getRawParameterValue (paramVoiceB)->load());
    juce::ignoreUnused (apvts.getRawParameterValue (paramDetune)->load());

    updateFilters (hpf, lpf);

    const auto [voiceAL, voiceAR] = panToGains (-spread);
    const auto [voiceBL, voiceBR] = panToGains ( spread);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& hp = hpfFilters[(size_t) ch];
        auto& lp = lpfFilters[(size_t) ch];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = lp.processSample (hp.processSample (data[i]));
    }

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float wetL = dryBuffer.getSample (0, i) * voiceAL + dryBuffer.getSample (1, i) * voiceBL;
        float wetR = dryBuffer.getSample (0, i) * voiceAR + dryBuffer.getSample (1, i) * voiceBR;

        const float dryL = dryBuffer.getSample (0, i);
        const float dryR = dryBuffer.getNumChannels() > 1 ? dryBuffer.getSample (1, i) : dryL;

        buffer.setSample (0, i, wetL * mix + dryL * (1.0f - mix));
        if (buffer.getNumChannels() > 1)
            buffer.setSample (1, i, wetR * mix + dryR * (1.0f - mix));
    }
}

void PITDoubleStrikeAudioProcessor::ensureState (int numChannels, int numSamples)
{
    if (dryBuffer.getNumChannels() != numChannels || dryBuffer.getNumSamples() != numSamples)
        dryBuffer.setSize (numChannels, numSamples, false, false, true);

    auto ensureFilters = [numChannels](auto& filters)
    {
        if ((int) filters.size() < numChannels)
        {
            filters.resize (numChannels);
            for (auto& f : filters)
                f.reset();
        }
    };

    ensureFilters (hpfFilters);
    ensureFilters (lpfFilters);
}

void PITDoubleStrikeAudioProcessor::updateFilters (float hpf, float lpf)
{
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                        juce::jlimit (20.0f, 2000.0f, hpf),
                                                                        0.707f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (1000.0f, 20000.0f, lpf),
                                                                      0.707f);

    for (auto& f : hpfFilters)
        f.coefficients = hpCoeffs;
    for (auto& f : lpfFilters)
        f.coefficients = lpCoeffs;
}

std::pair<float, float> PITDoubleStrikeAudioProcessor::panToGains (float pan) const
{
    pan = juce::jlimit (-1.0f, 1.0f, pan);
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
    return { std::cos (angle), std::sin (angle) };
}

void PITDoubleStrikeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void PITDoubleStrikeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorEditor* PITDoubleStrikeAudioProcessor::createEditor()
{
    return new PITDoubleStrikeAudioProcessorEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
PITDoubleStrikeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramVoiceA, "Voice A Pitch", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 7.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramVoiceB, "Voice B Pitch", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), -5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramDetune, "Detune", juce::NormalisableRange<float> (-20.0f, 20.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramSpread, "Spread", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramHPF, "HPF", juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.45f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramLPF, "LPF", juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.01f, 0.45f), 14000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMix, "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.7f));

    return { params.begin(), params.end() };
}

//==============================================================================
PITDoubleStrikeAudioProcessorEditor::PITDoubleStrikeAudioProcessorEditor (PITDoubleStrikeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (voiceASlider, "Voice A");
    initSlider (voiceBSlider, "Voice B");
    initSlider (detuneSlider, "Detune");
    initSlider (spreadSlider, "Spread");
    initSlider (hpfSlider,    "HPF");
    initSlider (lpfSlider,    "LPF");
    initSlider (mixSlider,    "Mix");

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramVoiceA, voiceASlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramVoiceB, voiceBSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDetune, detuneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramSpread, spreadSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramHPF, hpfSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramLPF, lpfSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMix, mixSlider));

    setSize (720, 280);
}

void PITDoubleStrikeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("PIT Double Strike", getLocalBounds().removeFromTop (30),
                      juce::Justification::centred, 1);
}

void PITDoubleStrikeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (30);

    auto row1 = area.removeFromTop (120);
    auto row2 = area;
    const int width = row1.getWidth() / 3;

    voiceASlider.setBounds (row1.removeFromLeft (width).reduced (6));
    voiceBSlider.setBounds (row1.removeFromLeft (width).reduced (6));
    detuneSlider.setBounds (row1.removeFromLeft (width).reduced (6));

    spreadSlider.setBounds (row2.removeFromLeft (width).reduced (6));
    hpfSlider   .setBounds (row2.removeFromLeft (width).reduced (6));
    lpfSlider   .setBounds (row2.removeFromLeft (width).reduced (6));
    mixSlider   .setBounds (row2.removeFromLeft (width).withTrimmedLeft (6).withTrimmedRight (6));
}

void PITDoubleStrikeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PITDoubleStrikeAudioProcessor();
}
