juce_add_plugin(EQGuitarBodyEQ
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqguitarbodyeq
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EGBE
    FORMATS VST3
    PRODUCT_NAME "EQ Guitar Body"
)

juce_generate_juce_header(EQGuitarBodyEQ)

target_sources(EQGuitarBodyEQ PRIVATE
    EQGuitarBodyEQAudioProcessor.cpp
    EQGuitarBodyEQAudioProcessor.h
)

target_link_libraries(EQGuitarBodyEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
