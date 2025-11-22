juce_add_plugin(GRDTubeLine
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.tubeline
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GTLn
    FORMATS VST3
    PRODUCT_NAME "GRD Tube Line"
)

juce_generate_juce_header(GRDTubeLine)

target_sources(GRDTubeLine PRIVATE
    GRDTubeLineAudioProcessor.cpp
    GRDTubeLineAudioProcessor.h
)

target_link_libraries(GRDTubeLine PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
