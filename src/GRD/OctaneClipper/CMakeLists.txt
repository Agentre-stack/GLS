juce_add_plugin(GROctaneClipper
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.octaneclipper
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GOCl
    FORMATS VST3
    PRODUCT_NAME "GRD Octane Clipper"
)

juce_generate_juce_header(GROctaneClipper)

target_sources(GROctaneClipper PRIVATE
    GROctaneClipperAudioProcessor.cpp
    GROctaneClipperAudioProcessor.h
)

target_link_libraries(GROctaneClipper PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
