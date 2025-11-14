juce_add_plugin(PITDoubleStrike
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.doublestrike
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE PDSt
    FORMATS VST3
    PRODUCT_NAME "PIT Double Strike"
)

juce_generate_juce_header(PITDoubleStrike)

target_sources(PITDoubleStrike PRIVATE
    PITDoubleStrikeAudioProcessor.cpp
    PITDoubleStrikeAudioProcessor.h
)

target_link_libraries(PITDoubleStrike PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
