juce_add_plugin(EQSculptEQ
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqsculpteq
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE ESCQ
    FORMATS VST3
    PRODUCT_NAME "EQ Sculpt EQ"
)

juce_generate_juce_header(EQSculptEQ)

target_sources(EQSculptEQ PRIVATE
    EQSculptEQAudioProcessor.cpp
    EQSculptEQAudioProcessor.h
)

target_link_libraries(EQSculptEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
