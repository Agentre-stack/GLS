juce_add_plugin(MDLTempoLFO
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.tempolfo
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MTLf
    FORMATS VST3
    PRODUCT_NAME "MDL Tempo LFO"
)

juce_generate_juce_header(MDLTempoLFO)

target_sources(MDLTempoLFO PRIVATE
    MDLTempoLFOAudioProcessor.cpp
    MDLTempoLFOAudioProcessor.h
)

target_link_libraries(MDLTempoLFO PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
