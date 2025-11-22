juce_add_plugin(EQAirGlass
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqairglass
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EAGL
    FORMATS VST3
    PRODUCT_NAME "EQ Air Glass"
)

juce_generate_juce_header(EQAirGlass)

target_sources(EQAirGlass PRIVATE
    EQAirGlassAudioProcessor.cpp
    EQAirGlassAudioProcessor.h
)

target_link_libraries(EQAirGlass PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
