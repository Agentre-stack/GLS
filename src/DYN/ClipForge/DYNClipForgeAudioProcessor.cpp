#include "DYNClipForgeAudioProcessor.h"

DYNClipForgeAudioProcessor::DYNClipForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CLIP_FORGE", createParameterLayout())
{
}

void DYNClipForgeAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    updateFilters (apvts.getRawParameterValue ("pre_hpf")->load(),
                   apvts.getRawParameterValue ("post_tone")->load());
}

void DYNClipForgeAudioProcessor::releaseResources()
{
}

void DYNClipForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto ceilingDb = get ("ceiling");
    const auto clipBlend = juce::jlimit (0.0f, 1.0f, get ("clip_blend"));
    const auto knee      = get ("knee");
    const auto preHpf    = get ("pre_hpf");
    const auto postTone  = get ("post_tone");
    const auto output    = juce::Decibels::decibelsToGain (get ("output_trim"));

    updateFilters (preHpf, postTone);

    const auto ceilingGain = juce::Decibels::decibelsToGain (ceilingDb);
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            sample = preHpfFilter.processSample (sample);

            const float soft = softClip (sample, knee);
            const float hard = hardClip (sample, ceilingGain);
            sample = soft * (1.0f - clipBlend) + hard * clipBlend;

            sample = postToneFilter.processSample (sample);
            data[i] = sample * output;
        }
    }
}

void DYNClipForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNClipForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNClipForgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ceiling",   "Ceiling",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("clip_blend","Clip Blend",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("knee",      "Knee",
                                                                   juce::NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pre_hpf",   "Pre HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("post_tone", "Post Tone",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim","Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

DYNClipForgeAudioProcessorEditor::DYNClipForgeAudioProcessorEditor (DYNClipForgeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (ceilingSlider,  "Ceiling");
    make (clipBlendSlider,"Blend");
    make (kneeSlider,     "Knee");
    make (preHpfSlider,   "Pre HPF");
    make (postToneSlider, "Post Tone");
    make (outputSlider,   "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "ceiling", "clip_blend", "knee", "pre_hpf", "post_tone", "output_trim" };
    juce::Slider* sliders[]      = { &ceilingSlider, &clipBlendSlider, &kneeSlider, &preHpfSlider, &postToneSlider, &outputSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (600, 260);
}

void DYNClipForgeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNClipForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Clip Forge", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNClipForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto width = area.getWidth() / 3;

    ceilingSlider .setBounds (area.removeFromLeft (width).reduced (8));
    clipBlendSlider.setBounds (area.removeFromLeft (width).reduced (8));
    kneeSlider    .setBounds (area.removeFromLeft (width).reduced (8));

    preHpfSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    postToneSlider.setBounds (area.removeFromLeft (width).reduced (8));
    outputSlider  .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* DYNClipForgeAudioProcessor::createEditor()
{
    return new DYNClipForgeAudioProcessorEditor (*this);
}

void DYNClipForgeAudioProcessor::updateFilters (float preHpfFreq, float postTone)
{
    if (currentSampleRate <= 0.0)
        return;

    preHpfFilter.lockCoefficients()->setCoefficients (*juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                                                         juce::jlimit (20.0f, 400.0f, preHpfFreq)));

    const auto pivot = postTone >= 0.0f ? 4000.0f : 200.0f;
    const auto gain  = juce::Decibels::decibelsToGain (postTone * 6.0f);
    postToneFilter.lockCoefficients()->setCoefficients (*juce::dsp::IIR::Coefficients<float>::makeShelving (currentSampleRate,
                                                                                                           pivot,
                                                                                                           postTone >= 0.0f,
                                                                                                           0.707f,
                                                                                                           gain));
}

float DYNClipForgeAudioProcessor::softClip (float x, float knee)
{
    if (knee <= 0.0f)
        return juce::jlimit (-1.0f, 1.0f, x);

    const float threshold = 1.0f - juce::jlimit (0.0f, 1.0f, knee / 18.0f);
    if (std::abs (x) <= threshold)
        return x;

    const float sign = x >= 0.0f ? 1.0f : -1.0f;
    const float excess = std::abs (x) - threshold;
    return sign * (threshold + excess / (1.0f + excess * excess));
}

float DYNClipForgeAudioProcessor::hardClip (float x, float ceiling)
{
    const float limited = juce::jlimit (-ceiling, ceiling, x);
    return limited;
}
