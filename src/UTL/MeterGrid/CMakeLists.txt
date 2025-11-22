juce_add_plugin(UTLMeterGrid
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.metergrid
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE UMGd
    FORMATS VST3
    PRODUCT_NAME "UTL Meter Grid"
)

juce_generate_juce_header(UTLMeterGrid)

target_sources(UTLMeterGrid PRIVATE
    UTLMeterGridAudioProcessor.cpp
    UTLMeterGridAudioProcessor.h
)

target_link_libraries(UTLMeterGrid PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
