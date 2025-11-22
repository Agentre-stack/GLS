juce_add_plugin(EQTiltLine
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqtiltline
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE ETLN
    FORMATS VST3
    PRODUCT_NAME "EQ Tilt Line"
)

juce_generate_juce_header(EQTiltLine)

target_sources(EQTiltLine PRIVATE
    EQTiltLineAudioProcessor.cpp
    EQTiltLineAudioProcessor.h
)

target_link_libraries(EQTiltLine PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
