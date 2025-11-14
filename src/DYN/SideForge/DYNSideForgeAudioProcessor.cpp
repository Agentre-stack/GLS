#include "DYNSideForgeAudioProcessor.h"

DYNSideForgeAudioProcessor::DYNSideForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SIDE_FORGE", createParameterLayout())
{
}

void DYNSideForgeAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    gainSmoothed = 1.0f;
}

void DYNSideForgeAudioProcessor::releaseResources()
{
}

void DYNSideForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb  = read ("thresh");
    const auto ratio     = juce::jmax (1.0f, read ("ratio"));
    const auto attackMs  = read ("attack");
    const auto releaseMs = read ("release");
    const auto scHpf     = read ("sc_hpf");
    const auto scLpf     = read ("sc_lpf");
    const auto lookahead = read ("lookahead");
    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));

    ensureStateSize();
    dryBuffer.makeCopyOf (buffer, true);

    scHpfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, scHpf);
    scLpfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, scLpf);

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto thresholdGain= juce::Decibels::decibelsToGain (threshDb);

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const auto delaySamples = juce::roundToInt (lookahead * 0.001f * currentSampleRate);

    for (auto& state : channelStates)
        state.lookahead.setDelay (delaySamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float scSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            const float in = buffer.getSample (ch, sample);
            state.lookahead.pushSample (0, in);
            scSample += in * 0.5f;
        }

        scSample = scHpfFilter.processSample (scSample);
        scSample = scLpfFilter.processSample (scSample);
        scSample = std::abs (scSample);

        float targetGain = 1.0f;
        if (scSample > thresholdGain)
        {
            const auto over = juce::Decibels::gainToDecibels (scSample) - threshDb;
            const auto compressed = threshDb + over / ratio;
            const auto gainDb = compressed - juce::Decibels::gainToDecibels (scSample);
            targetGain = juce::Decibels::decibelsToGain (gainDb);
        }

        if (targetGain < gainSmoothed)
            gainSmoothed = attackCoeff * (gainSmoothed - targetGain) + targetGain;
        else
            gainSmoothed = releaseCoeff * (gainSmoothed - targetGain) + targetGain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            float delayed = state.lookahead.popSample (0);
            buffer.setSample (ch, sample, delayed * gainSmoothed);
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }
}

void DYNSideForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNSideForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNSideForgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",  "Threshold",
                                                                   juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",   "Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",  "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 1000.0f, 0.01f, 0.35f), 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sc_hpf",  "SC HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sc_lpf",  "SC LPF",
                                                                   juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.01f, 0.35f), 6000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lookahead","Lookahead",
                                                                   juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.35f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",     "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

DYNSideForgeAudioProcessorEditor::DYNSideForgeAudioProcessorEditor (DYNSideForgeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (threshSlider,   "Thresh");
    make (ratioSlider,    "Ratio");
    make (attackSlider,   "Attack");
    make (releaseSlider,  "Release");
    make (scHpfSlider,    "SC HPF");
    make (scLpfSlider,    "SC LPF");
    make (lookaheadSlider,"Lookahead");
    make (mixSlider,      "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "thresh", "ratio", "attack", "release", "sc_hpf", "sc_lpf", "lookahead", "mix" };
    juce::Slider* sliders[]      = { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                     &scHpfSlider, &scLpfSlider, &lookaheadSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (760, 320);
}

void DYNSideForgeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNSideForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Side Forge", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNSideForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto rowHeight = area.getHeight() / 2;

    auto topRow = area.removeFromTop (rowHeight);
    auto bottomRow = area.removeFromTop (rowHeight);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (topRow,    { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider });
    layoutRow (bottomRow, { &scHpfSlider, &scLpfSlider, &lookaheadSlider, &mixSlider });
}

juce::AudioProcessorEditor* DYNSideForgeAudioProcessor::createEditor()
{
    return new DYNSideForgeAudioProcessorEditor (*this);
}

void DYNSideForgeAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
    {
        channelStates.resize (requiredChannels);
        juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) 512, 1 };
        scHpfFilter.prepare (spec);
        scLpfFilter.prepare (spec);
        for (auto& state : channelStates)
        {
            state.lookahead.prepare (spec);
            state.lookahead.reset();
            state.envelope = 0.0f;
        }
    }
}
