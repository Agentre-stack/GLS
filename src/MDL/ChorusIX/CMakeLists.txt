juce_add_plugin(MDLChorusIX
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.chorusix
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MCIX
    FORMATS VST3
    PRODUCT_NAME "MDL Chorus IX"
)

juce_generate_juce_header(MDLChorusIX)

target_sources(MDLChorusIX PRIVATE
    MDLChorusIXAudioProcessor.cpp
    MDLChorusIXAudioProcessor.h
)

target_link_libraries(MDLChorusIX PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
