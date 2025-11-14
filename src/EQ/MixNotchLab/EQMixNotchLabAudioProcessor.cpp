#include "EQMixNotchLabAudioProcessor.h"

EQMixNotchLabAudioProcessor::EQMixNotchLabAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIX_NOTCH_LAB", createParameterLayout())
{
}

void EQMixNotchLabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
}

void EQMixNotchLabAudioProcessor::releaseResources()
{
}

void EQMixNotchLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto n1Freq  = get ("notch1_freq");
    const auto n1Q     = get ("notch1_q");
    const auto n1Depth = get ("notch1_depth");
    const auto n2Freq  = get ("notch2_freq");
    const auto n2Q     = get ("notch2_q");
    const auto n2Depth = get ("notch2_depth");
    const auto listenMode = static_cast<int> (apvts.getRawParameterValue ("listen_mode")->load());

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);
    notchPreview1.makeCopyOf (buffer, true);
    notchPreview2.makeCopyOf (buffer, true);
    updateFilters (n1Freq, n1Q, n1Depth, n2Freq, n2Q, n2Depth);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers()[ch], 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        notch1Filters[ch].process (ctx);
        notch2Filters[ch].process (ctx);

        juce::dsp::AudioBlock<float> preview1 (notchPreview1.getArrayOfWritePointers()[ch], 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> previewCtx1 (preview1);
        notch1PreviewFilters[ch].process (previewCtx1);

        juce::dsp::AudioBlock<float> preview2 (notchPreview2.getArrayOfWritePointers()[ch], 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> previewCtx2 (preview2);
        notch2PreviewFilters[ch].process (previewCtx2);
    }

    if (listenMode > 0)
    {
        if (listenMode == 1)
            buffer.makeCopyOf (notchPreview1, true);
        else
            buffer.makeCopyOf (notchPreview2, true);
    }
}

void EQMixNotchLabAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQMixNotchLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQMixNotchLabAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto freqRange  = juce::NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.4f);
    auto qRange     = juce::NormalisableRange<float> (1.0f, 30.0f, 0.001f, 0.5f);
    auto depthRange = juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("notch1_freq",  "Notch1 Freq",  freqRange, 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("notch1_q",     "Notch1 Q",     qRange,    5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("notch1_depth", "Notch1 Depth", depthRange,-18.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("notch2_freq",  "Notch2 Freq",  freqRange, 5000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("notch2_q",     "Notch2 Q",     qRange,    8.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("notch2_depth", "Notch2 Depth", depthRange,-18.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("listen_mode", "Listen Mode",
                                                                    juce::StringArray { "Normal", "Notch1", "Notch2" }, 0));

    return { params.begin(), params.end() };
}

EQMixNotchLabAudioProcessorEditor::EQMixNotchLabAudioProcessorEditor (EQMixNotchLabAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (notch1FreqSlider,  "Notch 1 Freq");
    make (notch1QSlider,     "Notch 1 Q");
    make (notch1DepthSlider, "Notch 1 Depth");
    make (notch2FreqSlider,  "Notch 2 Freq");
    make (notch2QSlider,     "Notch 2 Q");
    make (notch2DepthSlider, "Notch 2 Depth");

    listenModeBox.addItemList ({ "Normal", "Notch1", "Notch2" }, 1);
    addAndMakeVisible (listenModeBox);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "notch1_freq", "notch1_q", "notch1_depth",
                                  "notch2_freq", "notch2_q", "notch2_depth" };
    juce::Slider* sliders[] = { &notch1FreqSlider, &notch1QSlider, &notch1DepthSlider,
                                &notch2FreqSlider, &notch2QSlider, &notch2DepthSlider };

    for (int i = 0; i < ids.size(); ++i)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    listenModeAttachment = std::make_unique<ComboAttachment> (state, "listen_mode", listenModeBox);

    setSize (760, 320);
}

void EQMixNotchLabAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQMixNotchLabAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Mix Notch Lab", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQMixNotchLabAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    listenModeBox.setBounds (area.removeFromTop (30));

    auto top = area.removeFromTop (area.getHeight() / 2);
    auto width = top.getWidth() / 3;
    notch1FreqSlider .setBounds (top.removeFromLeft (width).reduced (8));
    notch1QSlider    .setBounds (top.removeFromLeft (width).reduced (8));
    notch1DepthSlider.setBounds (top.removeFromLeft (width).reduced (8));

    auto bottom = area;
    width = bottom.getWidth() / 3;
    notch2FreqSlider .setBounds (bottom.removeFromLeft (width).reduced (8));
    notch2QSlider    .setBounds (bottom.removeFromLeft (width).reduced (8));
    notch2DepthSlider.setBounds (bottom.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQMixNotchLabAudioProcessor::createEditor()
{
    return new EQMixNotchLabAudioProcessorEditor (*this);
}

void EQMixNotchLabAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    auto ensureVector = [&](auto& vec)
    {
        if ((int) vec.size() < numChannels)
        {
            juce::dsp::ProcessSpec spec { currentSampleRate,
                                          lastBlockSize > 0 ? lastBlockSize : 512u,
                                          1 };
            const int previous = (int) vec.size();
            vec.resize (numChannels);
            for (int ch = previous; ch < numChannels; ++ch)
            {
                vec[ch].prepare (spec);
                vec[ch].reset();
            }
        }
    };

    ensureVector (notch1Filters);
    ensureVector (notch2Filters);
    ensureVector (notch1PreviewFilters);
    ensureVector (notch2PreviewFilters);

    dryBuffer.setSize (numChannels, lastBlockSize, false, false, true);
    notchPreview1.setSize (numChannels, lastBlockSize, false, false, true);
    notchPreview2.setSize (numChannels, lastBlockSize, false, false, true);
}

void EQMixNotchLabAudioProcessor::updateFilters (float n1Freq, float n1Q, float n1Depth,
                                                 float n2Freq, float n2Q, float n2Depth)
{
    if (currentSampleRate <= 0.0)
        return;

    auto makeNotch = [&](float freq, float q, float depthDb)
    {
        const auto clampedFreq = juce::jlimit (20.0f, (float) (currentSampleRate * 0.49f), freq);
        const auto clampedQ    = juce::jlimit (1.0f, 30.0f, q);
        const float gain = juce::Decibels::decibelsToGain (depthDb);
        return juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate,
                                                                    clampedFreq,
                                                                    clampedQ,
                                                                    gain);
    };

    auto coeffs1 = makeNotch (n1Freq, n1Q, n1Depth);
    auto coeffs2 = makeNotch (n2Freq, n2Q, n2Depth);

    for (auto& filter : notch1Filters)
        filter.coefficients = coeffs1;
    for (auto& filter : notch2Filters)
        filter.coefficients = coeffs2;
    for (auto& filter : notch1PreviewFilters)
        filter.coefficients = coeffs1;
    for (auto& filter : notch2PreviewFilters)
        filter.coefficients = coeffs2;
}
