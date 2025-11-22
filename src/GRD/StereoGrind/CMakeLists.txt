juce_add_plugin(GRDStereoGrind
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.stereogrind
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GSGd
    FORMATS VST3
    PRODUCT_NAME "GRD Stereo Grind"
)

juce_generate_juce_header(GRDStereoGrind)

target_sources(GRDStereoGrind PRIVATE
    GRDStereoGrindAudioProcessor.cpp
    GRDStereoGrindAudioProcessor.h
)

target_link_libraries(GRDStereoGrind PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
