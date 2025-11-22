#include "DYNClipForgeAudioProcessor.h"

namespace
{
constexpr auto kStateId = "CLIP_FORGE";
}

const std::array<DYNClipForgeAudioProcessor::Preset, 3> DYNClipForgeAudioProcessor::presetBank {{
    { "Drum Smash", {
        { "ceiling",     -1.5f },
        { "clip_blend",   0.35f },
        { "knee",         8.0f },
        { "pre_hpf",     80.0f },
        { "post_tone",    0.35f },
        { "output_trim", -1.0f },
        { "input_trim",   0.0f },
        { "mix",          0.8f },
        { "ui_bypass",    0.0f }
    }},
    { "Mix Clamp", {
        { "ceiling",     -3.0f },
        { "clip_blend",   0.65f },
        { "knee",         4.0f },
        { "pre_hpf",     60.0f },
        { "post_tone",   -0.15f },
        { "output_trim",  0.0f },
        { "input_trim",   0.0f },
        { "mix",          0.65f },
        { "ui_bypass",    0.0f }
    }},
    { "Master Edge", {
        { "ceiling",     -0.5f },
        { "clip_blend",   0.25f },
        { "knee",        10.0f },
        { "pre_hpf",     40.0f },
        { "post_tone",    0.2f },
        { "output_trim", -0.5f },
        { "input_trim",  -0.5f },
        { "mix",          0.9f },
        { "ui_bypass",    0.0f }
    }}
}};

DYNClipForgeAudioProcessor::DYNClipForgeAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNClipForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  static_cast<juce::uint32> (juce::jmax (1, samplesPerBlock)),
                                  1 };
    preHpfFilter.prepare (spec);
    preHpfFilter.reset();
    postToneFilter.prepare (spec);
    postToneFilter.reset();
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

    const bool bypassed = apvts.getRawParameterValue ("ui_bypass")->load() > 0.5f;
    const auto input    = juce::Decibels::decibelsToGain (get ("input_trim"));
    const auto ceilingDb = get ("ceiling");
    const auto clipBlend = juce::jlimit (0.0f, 1.0f, get ("clip_blend"));
    const auto knee      = get ("knee");
    const auto preHpf    = get ("pre_hpf");
    const auto postTone  = get ("post_tone");
    const auto output    = juce::Decibels::decibelsToGain (get ("output_trim"));
    const auto mix       = juce::jlimit (0.0f, 1.0f, get ("mix"));

    if (bypassed)
        return;

    updateFilters (preHpf, postTone);

    const auto ceilingGain = juce::Decibels::decibelsToGain (ceilingDb);
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    dryBuffer.makeCopyOf (buffer, true);
    buffer.applyGain (input);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            sample = preHpfFilter.processSample (sample);

            const float soft = softClip (sample, knee);
            const float hard = hardClip (sample, ceilingGain);
            sample = soft * (1.0f - clipBlend) + hard * clipBlend;

            sample = postToneFilter.processSample (sample);
            const float processed = sample * output;
            data[i] = processed * mix + dry[i] * (1.0f - mix);
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

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim","Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
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
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNClipForgeAudioProcessorEditor::DYNClipForgeAudioProcessorEditor (DYNClipForgeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.ClipForge", "Clip Forge")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false) { initSlider (slider, label, macro); };

    make (ceilingSlider,  "Ceiling", true);
    make (clipBlendSlider,"Blend", true);
    make (kneeSlider,     "Knee");
    make (preHpfSlider,   "Pre HPF");
    make (postToneSlider, "Post Tone");
    make (outputSlider,   "Output");
    make (inputTrimSlider,"Input");
    make (mixSlider,      "Mix");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "ceiling", "clip_blend", "knee", "pre_hpf", "post_tone", "output_trim", "input_trim", "mix" };
    juce::Slider* sliders[]      = { &ceilingSlider, &clipBlendSlider, &kneeSlider, &preHpfSlider, &postToneSlider, &outputSlider, &inputTrimSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (740, 360);
}

void DYNClipForgeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto label = std::make_unique<juce::Label>();
    label->setText (name, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    sliderLabels.push_back (std::move (label));
}

void DYNClipForgeAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNClipForgeAudioProcessorEditor::layoutLabels()
{
    const std::array<juce::Slider*, 8> sliders { &ceilingSlider, &clipBlendSlider, &kneeSlider, &preHpfSlider,
                                                 &postToneSlider, &outputSlider, &inputTrimSlider, &mixSlider };
    for (size_t i = 0; i < sliders.size() && i < sliderLabels.size(); ++i)
    {
        auto* slider = sliders[i];
        auto* label = sliderLabels[i].get();
        if (slider != nullptr && label != nullptr)
        {
            auto bounds = slider->getBounds().withHeight (18).translated (0, -20);
            label->setBounds (bounds);
        }
    }
}

void DYNClipForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNClipForgeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto body = bounds.reduced (12);
    auto top = body.removeFromTop (juce::roundToInt (body.getHeight() * 0.55f)).reduced (8);
    auto bottom = body.reduced (8);

    auto macroWidth = top.getWidth() / 3;
    ceilingSlider .setBounds (top.removeFromLeft (macroWidth).reduced (6));
    clipBlendSlider.setBounds (top.removeFromLeft (macroWidth).reduced (6));
    kneeSlider    .setBounds (top.removeFromLeft (macroWidth).reduced (6));

    auto microWidth = bottom.getWidth() / 3;
    preHpfSlider  .setBounds (bottom.removeFromLeft (microWidth).reduced (6));
    postToneSlider.setBounds (bottom.removeFromLeft (microWidth).reduced (6));
    outputSlider  .setBounds (bottom.removeFromLeft (microWidth).reduced (6));

    auto footerArea = footerComponent.getBounds().reduced (24, 8);
    auto slot = footerArea.getWidth() / 3;
    inputTrimSlider.setBounds (footerArea.removeFromLeft (slot).reduced (8));
    mixSlider      .setBounds (footerArea.removeFromLeft (slot).reduced (8));
    bypassButton   .setBounds (footerArea.removeFromLeft (slot).reduced (8, 12));

    layoutLabels();
}

juce::AudioProcessorEditor* DYNClipForgeAudioProcessor::createEditor()
{
    return new DYNClipForgeAudioProcessorEditor (*this);
}

void DYNClipForgeAudioProcessor::updateFilters (float preHpfFreq, float postTone)
{
    if (currentSampleRate <= 0.0)
        return;

    auto preCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                        juce::jlimit (20.0f, 400.0f, preHpfFreq),
                                                                        0.707f);
    preHpfFilter.coefficients = preCoeffs;

    const bool boostHighs = postTone >= 0.0f;
    const float pivot = boostHighs ? 4000.0f : 200.0f;
    const float gain  = juce::Decibels::decibelsToGain (std::abs (postTone) * 6.0f);
    if (boostHighs)
        postToneFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, pivot, 0.707f, gain);
    else
        postToneFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, pivot, 0.707f, 1.0f / gain);
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

int DYNClipForgeAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNClipForgeAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNClipForgeAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNClipForgeAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return;

    const auto& preset = presetBank[(size_t) index];
    for (const auto& entry : preset.params)
    {
        if (auto* param = apvts.getParameter (entry.first))
        {
            auto norm = param->getNormalisableRange().convertTo0to1 (entry.second);
            param->setValueNotifyingHost (norm);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DYNClipForgeAudioProcessor();
}
