juce_add_plugin(MDLPhaseGrid
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.phasegrid
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MPHg
    FORMATS VST3
    PRODUCT_NAME "MDL Phase Grid"
)

juce_generate_juce_header(MDLPhaseGrid)

target_sources(MDLPhaseGrid PRIVATE
    MDLPhaseGridAudioProcessor.cpp
    MDLPhaseGridAudioProcessor.h
)

target_link_libraries(MDLPhaseGrid PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
