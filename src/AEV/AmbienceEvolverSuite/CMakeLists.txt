juce_add_plugin(AEVAmbienceEvolverSuite
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.ambienceevolver
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE AAES
    FORMATS VST3
    PRODUCT_NAME "AEV Ambience Evolver Suite"
)

juce_generate_juce_header(AEVAmbienceEvolverSuite)

target_sources(AEVAmbienceEvolverSuite PRIVATE
    AEVAmbienceEvolverSuiteAudioProcessor.cpp
    AEVAmbienceEvolverSuiteAudioProcessor.h
)

target_link_libraries(AEVAmbienceEvolverSuite PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
