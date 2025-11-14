#include "MDLChorusIXAudioProcessor.h"

MDLChorusIXAudioProcessor::MDLChorusIXAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHORUS_IX", createParameterLayout())
{
}

void MDLChorusIXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureVoiceState (getTotalNumOutputChannels(),
                      (int) std::round (apvts.getRawParameterValue ("voices")->load()));
}

void MDLChorusIXAudioProcessor::releaseResources()
{
}

void MDLChorusIXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int voices   = juce::jlimit (1, 8, (int) std::round (get ("voices")));
    const float rate   = juce::jlimit (0.05f, 5.0f, get ("rate"));
    const float depth  = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float spread = juce::jlimit (0.0f, 1.0f, get ("spread"));
    const float tone   = juce::jlimit (-1.0f, 1.0f, get ("tone"));
    const float mix    = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    ensureVoiceState (numChannels, voices);
    dryBuffer.makeCopyOf (buffer, true);
    updateToneFilter (tone);

    const float baseDelaySamples = currentSampleRate * 0.015f; // 15 ms
    const float depthSamples     = depth * currentSampleRate * 0.01f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        auto& voiceArray = channelVoices[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float chorusSample = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                auto& voice = voiceArray[v];
                const float lfo = std::sin (voice.phase + v * juce::MathConstants<float>::twoPi / (float) voices);
                const float modDelay = baseDelaySamples + depthSamples * lfo;
                voice.delay.setDelay (juce::jlimit (1.0f, (float) (currentSampleRate * 0.05f), modDelay));

                const float delayed = voice.delay.popSample (0);
                voice.delay.pushSample (0, dry[i]);

                chorusSample += delayed;

                const float voiceRate = rate * (1.0f + 0.1f * (float) v);
                voice.phase += voiceRate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
                if (voice.phase > juce::MathConstants<float>::twoPi)
                    voice.phase -= juce::MathConstants<float>::twoPi;
            }

            chorusSample /= (float) voices;
            float processed = toneFilters[ch % 2].processSample (chorusSample);

            const float pan = spread > 0.0f ? (ch == 0 ? -(spread) : spread) : 0.0f;
            const float panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            processed *= std::cos (panAngle);

            wet[i] = processed * mix + dry[i] * (1.0f - mix);
        }
    }
}

void MDLChorusIXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLChorusIXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLChorusIXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterInt>   ("voices", "Voices", 1, 8, 4));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rate",   "Rate",
                                                                   juce::NormalisableRange<float> (0.05f, 5.0f, 0.001f, 0.4f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth",  "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spread", "Spread",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone",   "Tone",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",    "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLChorusIXAudioProcessorEditor::MDLChorusIXAudioProcessorEditor (MDLChorusIXAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (voicesSlider,  "Voices");
    make (rateSlider,    "Rate");
    make (depthSlider,   "Depth");
    make (spreadSlider,  "Spread");
    make (toneSlider,    "Tone");
    make (mixSlider,     "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "voices", "rate", "depth", "spread", "tone", "mix" };
    juce::Slider* sliders[] = { &voicesSlider, &rateSlider, &depthSlider, &spreadSlider, &toneSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 280);
}

void MDLChorusIXAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLChorusIXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::dimgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Chorus IX", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLChorusIXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto width = area.getWidth() / 6;

    voicesSlider.setBounds (area.removeFromLeft (width).reduced (8));
    rateSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    depthSlider .setBounds (area.removeFromLeft (width).reduced (8));
    spreadSlider.setBounds (area.removeFromLeft (width).reduced (8));
    toneSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider   .setBounds (area.removeFromLeft (width).reduced (8));
}

void MDLChorusIXAudioProcessor::ensureVoiceState (int numChannels, int numVoices)
{
    if (numChannels <= 0 || numVoices <= 0)
        return;

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };

    if ((int) channelVoices.size() < numChannels)
        channelVoices.resize (numChannels);

    for (auto& voiceArray : channelVoices)
    {
        if ((int) voiceArray.size() < numVoices)
        {
            const auto previous = (int) voiceArray.size();
            voiceArray.resize (numVoices);
            for (int v = previous; v < numVoices; ++v)
            {
                voiceArray[v].delay.setMaximumDelayInSamples ((int) (currentSampleRate * 0.05f));
                voiceArray[v].delay.prepare (spec);
                voiceArray[v].delay.reset();
                voiceArray[v].phase = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            }
        }
    }
}

void MDLChorusIXAudioProcessor::updateToneFilter (float tone)
{
    if (currentSampleRate <= 0.0)
        return;

    const float freq = juce::jmap (tone, -1.0f, 1.0f, 1500.0f, 9000.0f);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                    juce::jlimit (500.0f, (float) (currentSampleRate * 0.49f), freq),
                                                                    0.8f);
    for (auto& filter : toneFilters)
    {
        filter.coefficients = coeffs;
        filter.reset();
    }
}
