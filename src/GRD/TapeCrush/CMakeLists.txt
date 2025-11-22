juce_add_plugin(GRDTapeCrush
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.tapecrush
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GTCr
    FORMATS VST3
    PRODUCT_NAME "GRD Tape Crush"
)

juce_generate_juce_header(GRDTapeCrush)

target_sources(GRDTapeCrush PRIVATE
    GRDTapeCrushAudioProcessor.cpp
    GRDTapeCrushAudioProcessor.h
)

target_link_libraries(GRDTapeCrush PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
