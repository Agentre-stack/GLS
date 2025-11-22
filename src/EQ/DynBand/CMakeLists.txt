juce_add_plugin(EQDynBand
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqdynband
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EDBD
    FORMATS VST3
    PRODUCT_NAME "EQ Dyn Band"
)

juce_generate_juce_header(EQDynBand)

target_sources(EQDynBand PRIVATE
    EQDynBandAudioProcessor.cpp
    EQDynBandAudioProcessor.h
)

target_link_libraries(EQDynBand PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
