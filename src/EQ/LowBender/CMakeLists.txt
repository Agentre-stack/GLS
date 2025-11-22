juce_add_plugin(EQLowBender
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqlowbender
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE ELBD
    FORMATS VST3
    PRODUCT_NAME "EQ Low Bender"
)

juce_generate_juce_header(EQLowBender)

target_sources(EQLowBender PRIVATE
    EQLowBenderAudioProcessor.cpp
    EQLowBenderAudioProcessor.h
)

target_link_libraries(EQLowBender PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
