juce_add_plugin(GLSXOverBus
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.xoverbus
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GXBs
    FORMATS VST3
    PRODUCT_NAME "GLS XOver Bus"
)

juce_generate_juce_header(GLSXOverBus)

target_sources(GLSXOverBus PRIVATE
    GLSXOverBusAudioProcessor.cpp
    GLSXOverBusAudioProcessor.h
)

target_link_libraries(GLSXOverBus PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
