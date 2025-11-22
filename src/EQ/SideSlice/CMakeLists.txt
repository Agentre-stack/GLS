juce_add_plugin(EQSideSlice
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqSideslice
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE ESSC
    FORMATS VST3
    PRODUCT_NAME "EQ Side Slice"
)

juce_generate_juce_header(EQSideSlice)

target_sources(EQSideSlice PRIVATE
    EQSideSliceAudioProcessor.cpp
    EQSideSliceAudioProcessor.h
)

target_link_libraries(EQSideSlice PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
