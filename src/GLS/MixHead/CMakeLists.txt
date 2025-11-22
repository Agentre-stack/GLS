juce_add_plugin(GLSMixHead
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.mixhead
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GMHd
    FORMATS VST3
    PRODUCT_NAME "GLS Mix Head"
)

juce_generate_juce_header(GLSMixHead)

target_sources(GLSMixHead PRIVATE
    GLSMixHeadAudioProcessor.cpp
    GLSMixHeadAudioProcessor.h
)

target_link_libraries(GLSMixHead PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
