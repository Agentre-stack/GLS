juce_add_plugin(DYNVocalPresenceComp
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynvocalpresencecomp
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DVPC
    FORMATS VST3
    PRODUCT_NAME "DYN Vocal Presence Comp"
)

juce_generate_juce_header(DYNVocalPresenceComp)

target_sources(DYNVocalPresenceComp PRIVATE
    DYNVocalPresenceCompAudioProcessor.cpp
    DYNVocalPresenceCompAudioProcessor.h
)

target_link_libraries(DYNVocalPresenceComp PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
