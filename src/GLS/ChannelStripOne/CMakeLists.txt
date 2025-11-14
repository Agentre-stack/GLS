juce_add_plugin(GLSChannelStripOne
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.channelstripone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GCSO
    FORMATS VST3
    PRODUCT_NAME "GLS Channel Strip One"
)

juce_generate_juce_header(GLSChannelStripOne)

target_sources(GLSChannelStripOne
    PRIVATE
        GLSChannelStripOneAudioProcessor.cpp
        GLSChannelStripOneAudioProcessor.h
)

target_link_libraries(GLSChannelStripOne
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
)
