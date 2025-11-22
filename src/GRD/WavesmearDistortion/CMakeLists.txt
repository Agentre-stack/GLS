juce_add_plugin(GRDWavesmearDistortion
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.wavesmear
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GWSM
    FORMATS VST3
    PRODUCT_NAME "GRD Wavesmear Distortion"
)

juce_generate_juce_header(GRDWavesmearDistortion)

target_sources(GRDWavesmearDistortion PRIVATE
    GRDWavesmearDistortionAudioProcessor.cpp
    GRDWavesmearDistortionAudioProcessor.h
)

target_link_libraries(GRDWavesmearDistortion PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
