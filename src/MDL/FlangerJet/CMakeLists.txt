juce_add_plugin(MDLFlangerJet
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.flangerjet
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MFJe
    FORMATS VST3
    PRODUCT_NAME "MDL Flanger Jet"
)

juce_generate_juce_header(MDLFlangerJet)

target_sources(MDLFlangerJet PRIVATE
    MDLFlangerJetAudioProcessor.cpp
    MDLFlangerJetAudioProcessor.h
)

target_link_libraries(MDLFlangerJet PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
