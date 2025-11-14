#include "GLSBusGlueAudioProcessor.h"

GLSBusGlueAudioProcessor::GLSBusGlueAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BUS_GLUE", createParameterLayout())
{
}

void GLSBusGlueAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    sidechainFilter.reset();
}

void GLSBusGlueAudioProcessor::releaseResources()
{
}

void GLSBusGlueAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto readParam = [this](auto id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb   = readParam ("thresh");
    const auto ratio      = juce::jmax (1.0f, readParam ("ratio"));
    const auto attackMs   = juce::jmax (0.1f, readParam ("attack"));
    const auto releaseMs  = juce::jmax (1.0f, readParam ("release"));
    const auto kneeDb     = juce::jmax (0.0f, readParam ("knee"));
    const auto scHpf      = readParam ("sc_hpf");
    const auto mix        = juce::jlimit (0.0f, 1.0f, readParam ("mix"));
    const auto outputTrim = juce::Decibels::decibelsToGain (readParam ("output"));

    dryBuffer.makeCopyOf (buffer, true);
    updateSidechainFilter (scHpf);

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float scSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            scSample += buffer.getSample (ch, sample);
        scSample /= juce::jmax (1, numChannels);

        scSample = sidechainFilter.processSample (scSample);
        scSample = std::abs (scSample);

        if (scSample > detectorEnvelope)
            detectorEnvelope = attackCoeff * detectorEnvelope + (1.0f - attackCoeff) * scSample;
        else
            detectorEnvelope = releaseCoeff * detectorEnvelope + (1.0f - releaseCoeff) * scSample;

        const auto levelDb = juce::Decibels::gainToDecibels (juce::jmax (detectorEnvelope, 1.0e-6f));
        const auto gainDb  = computeGainDb (levelDb, threshDb, ratio, kneeDb);
        const auto targetGain = juce::Decibels::decibelsToGain (gainDb);
        gainSmoothed += 0.05f * (targetGain - gainSmoothed);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            data[sample] *= gainSmoothed;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }

    if (outputTrim != 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.applyGain (ch, 0, numSamples, outputTrim);
    }
}

void GLSBusGlueAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSBusGlueAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSBusGlueAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange      = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto timeRange    = juce::NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.25f);
    auto releaseRange = juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",  "Threshold", dBRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",   "Ratio", juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",  "Attack", timeRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release", releaseRange, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("knee",    "Knee", juce::NormalisableRange<float> (0.0f, 18.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sc_hpf",  "SC HPF", juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",     "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output",  "Output", juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

GLSBusGlueAudioProcessorEditor::GLSBusGlueAudioProcessorEditor (GLSBusGlueAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& slider, const juce::String& name)
    {
        initialiseSlider (slider, name);
    };

    makeSlider (threshSlider,  "Thresh");
    makeSlider (ratioSlider,   "Ratio");
    makeSlider (attackSlider,  "Attack");
    makeSlider (releaseSlider, "Release");
    makeSlider (kneeSlider,    "Knee");
    makeSlider (scHpfSlider,   "SC HPF");
    makeSlider (mixSlider,     "Mix");
    makeSlider (outputSlider,  "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "thresh", "ratio", "attack", "release", "knee", "sc_hpf", "mix", "output" };
    juce::Slider* sliders[] = { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                &kneeSlider, &scHpfSlider, &mixSlider, &outputSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 300);
}

void GLSBusGlueAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSBusGlueAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Bus Glue", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSBusGlueAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto row = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (row, { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider });
    layoutRow (area, { &kneeSlider, &scHpfSlider, &mixSlider, &outputSlider });
}

juce::AudioProcessorEditor* GLSBusGlueAudioProcessor::createEditor()
{
    return new GLSBusGlueAudioProcessorEditor (*this);
}

void GLSBusGlueAudioProcessor::updateSidechainFilter (float frequency)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto freq = juce::jlimit (10.0f, (float) (currentSampleRate * 0.45), frequency);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, freq);
    sidechainFilter.coefficients = coeffs;
}

float GLSBusGlueAudioProcessor::computeGainDb (float inputLevelDb, float thresholdDb, float ratio, float kneeDb) const
{
    const float slope = 1.0f / ratio;

    if (kneeDb > 0.0f)
    {
        const float halfKnee = kneeDb * 0.5f;

        if (inputLevelDb <= thresholdDb - halfKnee)
            return 0.0f;

        if (inputLevelDb >= thresholdDb + halfKnee)
            return (thresholdDb - inputLevelDb) * (1.0f - slope);

        const float x = inputLevelDb - (thresholdDb - halfKnee);
        return - (1.0f - slope) * (x * x) / (2.0f * kneeDb);
    }

    if (inputLevelDb <= thresholdDb)
        return 0.0f;

    return (thresholdDb - inputLevelDb) * (1.0f - slope);
}
