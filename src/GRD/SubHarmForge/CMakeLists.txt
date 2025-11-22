juce_add_plugin(GRDSubHarmForge
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.subharmforge
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GSHF
    FORMATS VST3
    PRODUCT_NAME "GRD Sub Harm Forge"
)

juce_generate_juce_header(GRDSubHarmForge)

target_sources(GRDSubHarmForge PRIVATE
    GRDSubHarmForgeAudioProcessor.cpp
    GRDSubHarmForgeAudioProcessor.h
)

target_link_libraries(GRDSubHarmForge PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
