juce_add_plugin(GLSChannelPilot
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.channelpilot
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GCPt
    FORMATS VST3
    PRODUCT_NAME "GLS Channel Pilot"
)

juce_generate_juce_header(GLSChannelPilot)

target_sources(GLSChannelPilot
    PRIVATE
        GLSChannelPilotAudioProcessor.cpp
        GLSChannelPilotAudioProcessor.h
)

target_link_libraries(GLSChannelPilot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
)
