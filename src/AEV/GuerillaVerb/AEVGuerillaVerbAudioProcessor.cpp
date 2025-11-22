#include "AEVGuerillaVerbAudioProcessor.h"

AEVGuerillaVerbAudioProcessor::AEVGuerillaVerbAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GUERILLA_VERB", createParameterLayout())
{
}

void AEVGuerillaVerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);

    reverb.reset();
    modulationPhase = { 0.0f, 0.5f };
    ensureStateSize (getTotalNumOutputChannels(), (int) lastBlockSize);
    updateFilters (120.0f, 16000.0f);
}

void AEVGuerillaVerbAudioProcessor::releaseResources()
{
}

void AEVGuerillaVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto size        = get ("size");
    const auto predelayMs  = get ("predelay");
    const auto decay       = get ("decay");
    const auto erLevel     = get ("er_level");
    const auto density     = get ("density");
    const auto damping     = get ("damping");
    const auto modDepth    = get ("mod_depth");
    const auto modRate     = get ("mod_rate");
    const auto color       = get ("color");
    const auto hpf         = get ("hpf");
    const auto lpf         = get ("lpf");
    const auto width       = get ("width");
    const auto abMorph     = get ("ab_morph");
    const auto irBlend     = get ("ir_blend");
    const auto mix         = get ("mix");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels, numSamples);

    dryBuffer.makeCopyOf (buffer, true);
    workBuffer.makeCopyOf (buffer, true);
    diffusionBuffer.clear();

    const float delaySamples = juce::jlimit (0.0f, 2.0f, predelayMs * 0.001f) * (float) currentSampleRate;

    // Pre-delay and modulation
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = workBuffer.getWritePointer (ch);
        auto& delay = preDelayLines[ch % preDelayLines.size()];
        delay.setDelay (delaySamples);

        float phase = modulationPhase[ch % modulationPhase.size()];
        const float phaseInc = juce::MathConstants<float>::twoPi * juce::jlimit (0.05f, 10.0f, modRate) / (float) currentSampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const float delayed = delay.popSample (0);
            delay.pushSample (0, data[i]);
            const float modulation = 1.0f + std::sin (phase) * modDepth * 0.3f;
            data[i] = delayed * modulation;
            phase += phaseInc;
            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }

        modulationPhase[ch % modulationPhase.size()] = phase;
    }

    // Early reflections blend
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = workBuffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float early = dry[i] * erLevel * 0.4f;
            wet[i] = wet[i] * (1.0f - erLevel * 0.2f) + early;
        }
    }

    updateReverbParameters (size, decay, density, damping, width, erLevel);
    preDelaySnapshot.makeCopyOf (workBuffer, true);
    juce::dsp::AudioBlock<float> workBlock (workBuffer);
    reverb.process (juce::dsp::ProcessContextReplacing<float> (workBlock));

    // Diffusion path acts as convolution approximation
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* diffWrite = diffusionBuffer.getWritePointer (ch);
        const auto* source = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            diffWrite[i] = processDiffusion (ch, source[i], density, damping, irBlend);
    }

    // Combine algorithmic + diffusion
    juce::AudioBuffer<float> wetBuffer;
    wetBuffer.makeCopyOf (workBuffer, true);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = wetBuffer.getWritePointer (ch);
        const auto* diff = diffusionBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = juce::jmap (irBlend, wet[i], diff[i]);
    }

    updateFilters (hpf, lpf);
    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = wetBlock.getSingleChannelBlock ((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
        hpfFilters[ch % 2].process (ctx);
        lpfFilters[ch % 2].process (ctx);
    }

    // Color shaping: simple saturation-based tilt
    const float colorAmount = juce::jlimit (-1.0f, 1.0f, color);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = wetBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float bright = wet[i] - std::tanh (wet[i]);
            wet[i] += bright * colorAmount * 0.5f;
        }
    }

    // Morph between wet signal and pre-delayed input
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = wetBuffer.getWritePointer (ch);
        const auto* pre = preDelaySnapshot.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = juce::jmap (abMorph, wet[i], pre[i]);
    }

    applyWidth (wetBuffer, width);

    const float mixWet = juce::jlimit (0.0f, 1.0f, mix);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* dest = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        const auto* wet = wetBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            dest[i] = wet[i] * mixWet + dry[i] * (1.0f - mixWet);
    }
}

void AEVGuerillaVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void AEVGuerillaVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
AEVGuerillaVerbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const auto norm = [](float start, float end, float step = 0.01f, float skew = 0.35f)
    {
        return juce::NormalisableRange<float> (start, end, step, skew);
    };

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("size",      "Size",      norm (0.1f, 1.0f, 0.001f, 0.8f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("predelay",  "PreDelay",  norm (0.0f, 200.0f, 0.01f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("decay",     "Decay",     norm (0.1f, 15.0f, 0.01f, 0.6f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("er_level",  "ER Level",  norm (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("density",   "Density",   norm (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("damping",   "Damping",   norm (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mod_depth", "Mod Depth", norm (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mod_rate",   "Mod Rate",  norm (0.05f, 10.0f, 0.001f, 0.4f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("color",     "Color",     norm (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf",       "HPF",       norm (20.0f, 2000.0f, 0.01f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lpf",       "LPF",       norm (2000.0f, 20000.0f, 0.01f), 16000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",     "Width",     norm (0.0f, 1.5f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ab_morph",  "A/B Morph", norm (0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ir_blend",  "IR Blend",  norm (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",       "Mix",       norm (0.0f, 1.0f, 0.001f), 0.4f));

    return { params.begin(), params.end() };
}

AEVGuerillaVerbAudioProcessorEditor::AEVGuerillaVerbAudioProcessorEditor (AEVGuerillaVerbAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    const juce::StringArray paramIds {
        "size", "predelay", "decay", "er_level", "density", "damping",
        "mod_depth", "mod_rate", "color", "hpf", "lpf", "width",
        "ab_morph", "ir_blend", "mix"
    };

    const juce::StringArray labels {
        "Size", "PreDelay", "Decay", "ER", "Density", "Damping",
        "Mod Depth", "Mod Rate", "Color", "HPF", "LPF", "Width",
        "A/B", "IR Blend", "Mix"
    };

    const auto numParams = paramIds.size();
    for (int i = 0; i < numParams; ++i)
        addSlider (paramIds[i], labels[i]);

    setSize (900, 420);
}

void AEVGuerillaVerbAudioProcessorEditor::addSlider (const juce::String& paramId, const juce::String& labelText)
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    sliders.add (slider);
    addAndMakeVisible (slider);

    auto* label = new juce::Label();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    labels.add (label);
    addAndMakeVisible (label);

    attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (
        processorRef.getValueTreeState(), paramId, *slider));
}

void AEVGuerillaVerbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("AEV Guerilla Verb", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void AEVGuerillaVerbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto rowHeight = area.getHeight() / 3;

    int sliderIndex = 0;
    for (int row = 0; row < 3; ++row)
    {
        auto rowBounds = area.removeFromTop (rowHeight);
        auto width = rowBounds.getWidth() / 5;
        for (int col = 0; col < 5 && sliderIndex < sliders.size(); ++col, ++sliderIndex)
        {
            auto bounds = rowBounds.removeFromLeft (width).reduced (8);
            sliders[sliderIndex]->setBounds (bounds.removeFromTop (bounds.getHeight() - 20));
            labels[sliderIndex]->setBounds (bounds);
        }
    }
}

juce::AudioProcessorEditor* AEVGuerillaVerbAudioProcessor::createEditor()
{
    return new AEVGuerillaVerbAudioProcessorEditor (*this);
}

void AEVGuerillaVerbAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    const auto requiredChannels = juce::jmax (0, numChannels);
    const auto samples = juce::jmax (1, numSamples > 0 ? numSamples : (int) lastBlockSize);

    if (requiredChannels <= 0)
    {
        diffusers.clear();
        dryBuffer.setSize (0, 0);
        workBuffer.setSize (0, 0);
        diffusionBuffer.setSize (0, 0);
        preDelaySnapshot.setSize (0, 0);
        return;
    }

    diffusers.resize ((size_t) requiredChannels);

    juce::dsp::ProcessSpec spec {
        currentSampleRate > 0.0 ? currentSampleRate : 44100.0,
        (juce::uint32) samples,
        1
    };

    for (auto& channelDiffusers : diffusers)
        for (auto& diff : channelDiffusers)
        {
            diff.line.prepare (spec);
            diff.line.reset();
            diff.feedback = 0.5f;
        }

    for (auto& line : preDelayLines)
    {
        line.prepare (spec);
        line.reset();
    }

    dryBuffer.setSize (requiredChannels, samples, false, false, true);
    workBuffer.setSize (requiredChannels, samples, false, false, true);
    diffusionBuffer.setSize (requiredChannels, samples, false, false, true);
    preDelaySnapshot.setSize (requiredChannels, samples, false, false, true);
    diffusionBuffer.clear();
}

void AEVGuerillaVerbAudioProcessor::updateFilters (float hpf, float lpf)
{
    if (currentSampleRate <= 0.0)
        return;

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                       juce::jlimit (20.0f, 5000.0f, hpf),
                                                                       0.707f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (2000.0f, (float) (currentSampleRate * 0.49f), lpf),
                                                                      0.707f);
    for (auto& filter : hpfFilters)
        filter.coefficients = hpCoeffs;
    for (auto& filter : lpfFilters)
        filter.coefficients = lpCoeffs;
}

void AEVGuerillaVerbAudioProcessor::updateReverbParameters (float size, float decay, float density, float damping, float width, float erLevel)
{
    juce::dsp::Reverb::Parameters params;
    const float decayFactor = juce::jlimit (0.1f, 1.0f, decay / 10.0f);
    params.roomSize = juce::jlimit (0.1f, 1.0f, size * decayFactor);
    params.damping = juce::jlimit (0.0f, 1.0f, damping);
    params.wetLevel = juce::jlimit (0.0f, 1.0f, 0.3f + density * 0.5f);
    params.dryLevel = juce::jlimit (0.0f, 1.0f, 1.0f - params.wetLevel);
    params.width = juce::jlimit (0.0f, 1.0f, width * 0.8f);
    params.freezeMode = 0.0f;
    reverb.setParameters (params);
    juce::ignoreUnused (erLevel);
}

float AEVGuerillaVerbAudioProcessor::processDiffusion (int channel, float input, float density, float damping, float irBlend)
{
    if (channel >= (int) diffusers.size())
        return 0.0f;

    auto& chain = diffusers[channel];
    float sum = 0.0f;
    const float baseFeedback = 0.25f + density * 0.5f;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        auto& diff = chain[i];
        const float delay = juce::jlimit (10.0f, (float) maxDelaySamples - 1.0f,
                                          600.0f + (float) i * 700.0f + density * 3500.0f);
        diff.line.setDelay (delay);
        diff.feedback = juce::jlimit (0.1f, 0.95f, baseFeedback - 0.05f * (float) i);
        auto delayed = diff.line.popSample (0);
        const float damped = delayed * (1.0f - damping * 0.5f);
        diff.line.pushSample (0, input + damped * diff.feedback);
        sum += delayed;
    }
    juce::ignoreUnused (irBlend);
    return sum * 0.3f;
}

void AEVGuerillaVerbAudioProcessor::applyWidth (juce::AudioBuffer<float>& buffer, float widthAmount)
{
    if (buffer.getNumChannels() < 2)
        return;

    const float width = juce::jlimit (0.0f, 2.0f, widthAmount);
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const int numSamples = buffer.getNumSamples();
    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = 0.5f * (left[i] + right[i]);
        const float side = 0.5f * (left[i] - right[i]) * width;
        left[i]  = mid + side;
        right[i] = mid - side;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AEVGuerillaVerbAudioProcessor();
}
