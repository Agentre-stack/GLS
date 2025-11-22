juce_add_plugin(DYNTransFix
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.transfix
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DTFx
    FORMATS VST3
    PRODUCT_NAME "DYN Trans Fix"
)

juce_generate_juce_header(DYNTransFix)

target_sources(DYNTransFix PRIVATE
    DYNTransFixAudioProcessor.cpp
    DYNTransFixAudioProcessor.h
)

target_link_libraries(DYNTransFix PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
