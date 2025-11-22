juce_add_plugin(DYNVocalPin
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.vocalpin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DVPn
    FORMATS VST3
    PRODUCT_NAME "DYN Vocal Pin"
)

juce_generate_juce_header(DYNVocalPin)

target_sources(DYNVocalPin PRIVATE
    DYNVocalPinAudioProcessor.cpp
    DYNVocalPinAudioProcessor.h
)

target_link_libraries(DYNVocalPin PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
