juce_add_plugin(GLSSubCommand
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.subcommand
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GSbC
    FORMATS VST3
    PRODUCT_NAME "GLS Sub Command"
)

juce_generate_juce_header(GLSSubCommand)

target_sources(GLSSubCommand PRIVATE
    GLSSubCommandAudioProcessor.cpp
    GLSSubCommandAudioProcessor.h
)

target_link_libraries(GLSSubCommand PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
