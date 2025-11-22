juce_add_plugin(GLSBusGlue
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.busglue
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GBGl
    FORMATS VST3
    PRODUCT_NAME "GLS Bus Glue"
)

juce_generate_juce_header(GLSBusGlue)

target_sources(GLSBusGlue PRIVATE
    GLSBusGlueAudioProcessor.cpp
    GLSBusGlueAudioProcessor.h
)

target_link_libraries(GLSBusGlue PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
