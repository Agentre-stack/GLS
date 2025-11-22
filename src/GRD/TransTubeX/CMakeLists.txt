juce_add_plugin(GRDTransTubeX
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.transtubex
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GTTX
    FORMATS VST3
    PRODUCT_NAME "GRD Trans Tube X"
)

juce_generate_juce_header(GRDTransTubeX)

target_sources(GRDTransTubeX PRIVATE
    GRDTransTubeXAudioProcessor.cpp
    GRDTransTubeXAudioProcessor.h
)

target_link_libraries(GRDTransTubeX PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
