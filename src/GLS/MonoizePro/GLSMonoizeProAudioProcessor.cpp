#include "GLSMonoizeProAudioProcessor.h"

GLSMonoizeProAudioProcessor::GLSMonoizeProAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONOIZE_PRO", createParameterLayout())
{
}

void GLSMonoizeProAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    updateFilters (apvts.getRawParameterValue ("mono_below")->load(),
                   apvts.getRawParameterValue ("stereo_above")->load());
}

void GLSMonoizeProAudioProcessor::releaseResources()
{
}

void GLSMonoizeProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto monoBelow   = read ("mono_below");
    const auto stereoAbove = read ("stereo_above");
    const auto widthParam  = juce::jlimit (0.0f, 2.0f, read ("width"));
    const auto centerLift  = juce::Decibels::decibelsToGain (read ("center_lift"));
    const auto sideTrim    = juce::Decibels::decibelsToGain (read ("side_trim"));

    updateFilters (monoBelow, stereoAbove);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float mid  = 0.5f * (left[i] + right[i]);
        float side = 0.5f * (left[i] - right[i]);

        float lowSide = monoLowFilter.processSample (side);
        float highSide = stereoHighFilter.processSample (side);
        float bandSide = side - lowSide - highSide;

        lowSide = 0.0f;
        highSide *= 1.0f + juce::jlimit (0.0f, 1.0f, (stereoAbove - 1000.0f) / 11000.0f);

        side = lowSide + bandSide + highSide;

        mid *= centerLift;
        side *= sideTrim * widthParam;

        left[i]  = mid + side;
        right[i] = mid - side;
    }
}

void GLSMonoizeProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSMonoizeProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSMonoizeProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mono_below",   "Mono Below",
                                                                   juce::NormalisableRange<float> (40.0f, 400.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("stereo_above", "Stereo Above",
                                                                   juce::NormalisableRange<float> (1000.0f, 12000.0f, 0.01f, 0.35f), 3000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",        "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("center_lift",  "Center Lift",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("side_trim",    "Side Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

GLSMonoizeProAudioProcessorEditor::GLSMonoizeProAudioProcessorEditor (GLSMonoizeProAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& name) { initialiseSlider (slider, name); };

    make (monoBelowSlider,   "Mono Below");
    make (stereoAboveSlider, "Stereo Above");
    make (widthSlider,       "Width");
    make (centerLiftSlider,  "Center Lift");
    make (sideTrimSlider,    "Side Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "mono_below", "stereo_above", "width", "center_lift", "side_trim" };
    juce::Slider* sliders[]      = { &monoBelowSlider, &stereoAboveSlider, &widthSlider, &centerLiftSlider, &sideTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (620, 260);
}

void GLSMonoizeProAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSMonoizeProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Monoize Pro", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSMonoizeProAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    monoBelowSlider .setBounds (area.removeFromLeft (width).reduced (8));
    stereoAboveSlider.setBounds (area.removeFromLeft (width).reduced (8));
    widthSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    centerLiftSlider.setBounds (area.removeFromLeft (width).reduced (8));
    sideTrimSlider  .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GLSMonoizeProAudioProcessor::createEditor()
{
    return new GLSMonoizeProAudioProcessorEditor (*this);
}

void GLSMonoizeProAudioProcessor::updateFilters (float monoFreq, float stereoFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (20.0f, (float) (currentSampleRate * 0.45), monoFreq));
    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                       juce::jlimit (100.0f, (float) (currentSampleRate * 0.45), stereoFreq));

    monoLowFilter.coefficients = lp;
    stereoHighFilter.coefficients = hp;
}
