juce_add_plugin(UTLPhaseOrb
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.phaseorb
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE UPRB
    FORMATS VST3
    PRODUCT_NAME "UTL PhaseOrb"
)

juce_generate_juce_header(UTLPhaseOrb)

target_sources(UTLPhaseOrb PRIVATE
    UTLPhaseOrbAudioProcessor.cpp
    UTLPhaseOrbAudioProcessor.h
)

target_link_libraries(UTLPhaseOrb PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
