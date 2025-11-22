juce_add_plugin(UTLBandRouter
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.bandrouter
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE UBRt
    FORMATS VST3
    PRODUCT_NAME "UTL Band Router"
)

juce_generate_juce_header(UTLBandRouter)

target_sources(UTLBandRouter PRIVATE
    UTLBandRouterAudioProcessor.cpp
    UTLBandRouterAudioProcessor.h
)

target_link_libraries(UTLBandRouter PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
