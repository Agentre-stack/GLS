juce_add_plugin(DYNSideForge
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynsideforge
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DSFG
    FORMATS VST3
    PRODUCT_NAME "DYN Side Forge"
)

juce_generate_juce_header(DYNSideForge)

target_sources(DYNSideForge PRIVATE
    DYNSideForgeAudioProcessor.cpp
    DYNSideForgeAudioProcessor.h
)

target_link_libraries(DYNSideForge PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
