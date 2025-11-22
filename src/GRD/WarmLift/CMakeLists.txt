juce_add_plugin(GRDWarmLift
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.warmlift
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GWLf
    FORMATS VST3
    PRODUCT_NAME "GRD Warm Lift"
)

juce_generate_juce_header(GRDWarmLift)

target_sources(GRDWarmLift PRIVATE
    GRDWarmLiftAudioProcessor.cpp
    GRDWarmLiftAudioProcessor.h
)

target_link_libraries(GRDWarmLift PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
