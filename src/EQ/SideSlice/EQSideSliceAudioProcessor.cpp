#include "EQSideSliceAudioProcessor.h"

EQSideSliceAudioProcessor::EQSideSliceAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SIDE_SLICE", createParameterLayout())
{
}

void EQSideSliceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    prepareFilters (currentSampleRate, samplesPerBlock);
}

void EQSideSliceAudioProcessor::releaseResources()
{
}

void EQSideSliceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto modeIndex  = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    const auto midBandDb  = get ("mid_band");
    const auto sideBandDb = get ("side_band");
    const auto midTrimDb  = get ("mid_trim");
    const auto sideTrimDb = get ("side_trim");
    const auto width      = juce::jlimit (0.0f, 2.0f, get ("width"));

    const float midTrimGain  = juce::Decibels::decibelsToGain (midTrimDb);
    const float sideTrimGain = juce::Decibels::decibelsToGain (sideTrimDb);

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    prepareFilters (currentSampleRate, (int) lastBlockSize);
    updateFilterCoefficients (midBandDb, sideBandDb);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (modeIndex == 1 && numChannels >= 2) // MS mode
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            float mid  = 0.5f * (left[i] + right[i]);
            float side = 0.5f * (left[i] - right[i]);

            mid  = midFilter.processSample (mid) * midTrimGain;
            side = sideFilter.processSample (side) * sideTrimGain * width;

            left[i]  = mid + side;
            right[i] = mid - side;
        }
    }
    else // Stereo mode or mono
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& filter = stereoFilters[juce::jmin (ch, 1)];
            const bool isLeft = (ch % 2 == 0);
            const float gain = (isLeft ? midTrimGain : sideTrimGain) * (isLeft ? 1.0f : width);

            for (int i = 0; i < numSamples; ++i)
                data[i] = filter.processSample (data[i]) * gain;
        }

        if (numChannels == 1)
        {
            auto* mono = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                mono[i] = midFilter.processSample (mono[i]) * midTrimGain;
        }
    }
}

void EQSideSliceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQSideSliceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQSideSliceAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode",
                                                                    juce::StringArray { "Stereo", "MS" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mid_band",  "Mid Band",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("side_band", "Side Band",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mid_trim",  "Mid Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("side_trim", "Side Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",    "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

EQSideSliceAudioProcessorEditor::EQSideSliceAudioProcessorEditor (EQSideSliceAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    modeBox.addItemList ({ "Stereo", "MS" }, 1);
    addAndMakeVisible (modeBox);

    initSlider (midBandSlider,  "Mid Band");
    initSlider (sideBandSlider, "Side Band");
    initSlider (midTrimSlider,  "Mid Trim");
    initSlider (sideTrimSlider, "Side Trim");
    initSlider (widthSlider,    "Width");

    auto& state = processorRef.getValueTreeState();
    modeAttachment = std::make_unique<ComboBoxAttachment> (state, "mode", modeBox);

    const juce::StringArray ids { "mid_band", "side_band", "mid_trim", "side_trim", "width" };
    juce::Slider* sliders[]      = { &midBandSlider, &sideBandSlider, &midTrimSlider, &sideTrimSlider, &widthSlider };

    for (int i = 0; i < ids.size(); ++i)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (680, 260);
}

void EQSideSliceAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQSideSliceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Side Slice", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQSideSliceAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    modeBox.setBounds (area.removeFromTop (30));

    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 5;
    midBandSlider .setBounds (row.removeFromLeft (width).reduced (8));
    sideBandSlider.setBounds (row.removeFromLeft (width).reduced (8));
    midTrimSlider .setBounds (row.removeFromLeft (width).reduced (8));
    sideTrimSlider.setBounds (row.removeFromLeft (width).reduced (8));
    widthSlider   .setBounds (row.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQSideSliceAudioProcessor::createEditor()
{
    return new EQSideSliceAudioProcessorEditor (*this);
}

void EQSideSliceAudioProcessor::prepareFilters (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) juce::jmax (1, samplesPerBlock),
                                  1 };
    midFilter.prepare (spec);
    midFilter.reset();
    sideFilter.prepare (spec);
    sideFilter.reset();
    for (auto& filter : stereoFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
}

void EQSideSliceAudioProcessor::updateFilterCoefficients (float midBandDb, float sideBandDb)
{
    if (currentSampleRate <= 0.0)
        return;

    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 400.0f, 0.8f,
                                                                          juce::Decibels::decibelsToGain (midBandDb));
    auto sideCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 2500.0f, 0.9f,
                                                                           juce::Decibels::decibelsToGain (sideBandDb));

    midFilter.coefficients = midCoeffs;
    stereoFilters[0].coefficients = midCoeffs;
    sideFilter.coefficients = sideCoeffs;
    stereoFilters[1].coefficients = sideCoeffs;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQSideSliceAudioProcessor();
}
