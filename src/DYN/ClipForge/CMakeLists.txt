juce_add_plugin(DYNClipForge
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynclipforge
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DCFG
    FORMATS VST3
    PRODUCT_NAME "DYN Clip Forge"
)

juce_generate_juce_header(DYNClipForge)

target_sources(DYNClipForge PRIVATE
    DYNClipForgeAudioProcessor.cpp
    DYNClipForgeAudioProcessor.h
)

target_link_libraries(DYNClipForge PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
