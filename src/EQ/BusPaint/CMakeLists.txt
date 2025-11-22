juce_add_plugin(EQBusPaint
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqbuspaint
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EBPT
    FORMATS VST3
    PRODUCT_NAME "EQ Bus Paint"
)

juce_generate_juce_header(EQBusPaint)

target_sources(EQBusPaint PRIVATE
    EQBusPaintAudioProcessor.cpp
    EQBusPaintAudioProcessor.h
)

target_link_libraries(EQBusPaint PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
