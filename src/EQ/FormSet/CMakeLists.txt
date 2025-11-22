juce_add_plugin(EQFormSet
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqformset
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EFRM
    FORMATS VST3
    PRODUCT_NAME "EQ Form Set"
)

juce_generate_juce_header(EQFormSet)

target_sources(EQFormSet PRIVATE
    EQFormSetAudioProcessor.cpp
    EQFormSetAudioProcessor.h
)

target_link_libraries(EQFormSet PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
