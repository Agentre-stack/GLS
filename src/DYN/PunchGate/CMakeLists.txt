juce_add_plugin(DYNPunchGate
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynpunchgate
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DPGT
    FORMATS VST3
    PRODUCT_NAME "DYN Punch Gate"
)

juce_generate_juce_header(DYNPunchGate)

target_sources(DYNPunchGate PRIVATE
    DYNPunchGateAudioProcessor.cpp
    DYNPunchGateAudioProcessor.h
)

target_link_libraries(DYNPunchGate PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
