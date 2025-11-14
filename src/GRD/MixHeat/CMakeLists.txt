juce_add_plugin(GRDMixHeat
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.mixheat
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GMHt
    FORMATS VST3
    PRODUCT_NAME "GRD Mix Heat"
)

juce_generate_juce_header(GRDMixHeat)

target_sources(GRDMixHeat
    PRIVATE
        GRDMixHeatAudioProcessor.cpp
        GRDMixHeatAudioProcessor.h
)

target_link_libraries(GRDMixHeat
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
)
