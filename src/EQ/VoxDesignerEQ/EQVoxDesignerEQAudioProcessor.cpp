#include "EQVoxDesignerEQAudioProcessor.h"

EQVoxDesignerEQAudioProcessor::EQVoxDesignerEQAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOX_DESIGNER_EQ", createParameterLayout())
{
}

void EQVoxDesignerEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
}

void EQVoxDesignerEQAudioProcessor::releaseResources()
{
}

void EQVoxDesignerEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto chestGain     = get ("chest_gain");
    const auto presenceGain  = get ("presence_gain");
    const auto sibilanceTame = juce::jlimit (0.0f, 1.0f, get ("sibilance_tame"));
    const auto airGain       = get ("air_gain");
    const auto exciter       = juce::jlimit (0.0f, 1.0f, get ("exciter"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);
    updateFilters (chestGain, presenceGain, airGain);

    const float exciterDrive = 1.0f + exciter * 2.0f;
    const float sibilanceThreshold = juce::Decibels::decibelsToGain (-12.0f);
    const float attackCoeff  = std::exp (-1.0f / (0.0025f * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (0.08f * (float) currentSampleRate));

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* dry  = dryBuffer.getReadPointer (ch);
        auto& sibilanceEnv = sibilanceEnvelopes[ch];

        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);
        {
            juce::dsp::ProcessContextReplacing<float> chestCtx (channelBlock);
            chestShelves[ch].process (chestCtx);
        }
        {
            juce::dsp::ProcessContextReplacing<float> presenceCtx (channelBlock);
            presenceBells[ch].process (presenceCtx);
        }
        {
            juce::dsp::ProcessContextReplacing<float> airCtx (channelBlock);
            airShelves[ch].process (airCtx);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float sibSample = sibilanceFilters[ch].processSample (data[i]);
            const float level = std::abs (sibSample);
            if (level > sibilanceEnv)
                sibilanceEnv = attackCoeff * sibilanceEnv + (1.0f - attackCoeff) * level;
            else
                sibilanceEnv = releaseCoeff * sibilanceEnv + (1.0f - releaseCoeff) * level;

            const float reduction = sibilanceTame * juce::jlimit (0.0f, 1.0f, (sibilanceEnv - sibilanceThreshold) * 4.0f);
            data[i] -= sibSample * reduction;

            const float exciterHp = exciterHighpasses[ch].processSample (dry[i]);
            const float excited = std::tanh (exciterHp * exciterDrive);
            data[i] += excited * exciter * 0.4f;
        }
    }
}

void EQVoxDesignerEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQVoxDesignerEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQVoxDesignerEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("chest_gain",    "Chest Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence_gain", "Presence Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sibilance_tame","Sibilance Tame",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_gain",      "Air Gain",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("exciter",       "Exciter",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

    return { params.begin(), params.end() };
}

EQVoxDesignerEQAudioProcessorEditor::EQVoxDesignerEQAudioProcessorEditor (EQVoxDesignerEQAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label)
    {
        initSlider (slider, label);
    };

    make (chestSlider,   "Chest");
    make (presenceSlider,"Presence");
    make (sibilanceSlider,"Sibilance");
    make (airSlider,     "Air");
    make (exciterSlider, "Exciter");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "chest_gain", "presence_gain", "sibilance_tame", "air_gain", "exciter" };
    juce::Slider* sliders[]      = { &chestSlider, &presenceSlider, &sibilanceSlider, &airSlider, &exciterSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 260);
}

void EQVoxDesignerEQAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQVoxDesignerEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Vox Designer", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQVoxDesignerEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    chestSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    presenceSlider.setBounds (area.removeFromLeft (width).reduced (8));
    sibilanceSlider.setBounds (area.removeFromLeft (width).reduced (8));
    airSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    exciterSlider .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQVoxDesignerEQAudioProcessor::createEditor()
{
    return new EQVoxDesignerEQAudioProcessorEditor (*this);
}

void EQVoxDesignerEQAudioProcessor::ensureStateSize (int numChannels)
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

    ensureVector (chestShelves);
    ensureVector (presenceBells);
    ensureVector (sibilanceFilters);
    ensureVector (airShelves);
    ensureVector (exciterHighpasses);
    if ((int) sibilanceEnvelopes.size() < numChannels)
        sibilanceEnvelopes.resize (numChannels, 0.0f);

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };
    auto reprepare = [&](auto& vec)
    {
        for (auto& filter : vec)
        {
            filter.prepare (spec);
            filter.reset();
        }
    };

    reprepare (chestShelves);
    reprepare (presenceBells);
    reprepare (sibilanceFilters);
    reprepare (airShelves);
    reprepare (exciterHighpasses);
}

void EQVoxDesignerEQAudioProcessor::updateFilters (float chestGain, float presenceGain, float airGain)
{
    if (currentSampleRate <= 0.0)
        return;

    auto chestCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 180.0f, 0.8f,
                                                                          juce::Decibels::decibelsToGain (chestGain));
    auto presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 3200.0f, 1.2f,
                                                                               juce::Decibels::decibelsToGain (presenceGain));
    auto sibilanceCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, 6500.0f, 2.5f);
    auto airCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 9000.0f, 0.8f,
                                                                         juce::Decibels::decibelsToGain (airGain));
    auto exciterCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 5000.0f, 0.707f);

    for (auto& f : chestShelves)
        f.coefficients = chestCoeffs;
    for (auto& f : presenceBells)
        f.coefficients = presenceCoeffs;
    for (auto& f : sibilanceFilters)
        f.coefficients = sibilanceCoeffs;
    for (auto& f : airShelves)
        f.coefficients = airCoeffs;
    for (auto& f : exciterHighpasses)
        f.coefficients = exciterCoeffs;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQVoxDesignerEQAudioProcessor();
}
