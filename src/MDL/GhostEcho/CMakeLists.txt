juce_add_plugin(MDLGhostEcho
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.ghostecho
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MGEc
    FORMATS VST3
    PRODUCT_NAME "MDL Ghost Echo"
)

juce_generate_juce_header(MDLGhostEcho)

target_sources(MDLGhostEcho PRIVATE
    MDLGhostEchoAudioProcessor.cpp
    MDLGhostEchoAudioProcessor.h
)

target_link_libraries(MDLGhostEcho PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
