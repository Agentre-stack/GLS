juce_add_plugin(GLSMixGuard
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.mixguard
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GMGr
    FORMATS VST3
    PRODUCT_NAME "GLS Mix Guard"
)

juce_generate_juce_header(GLSMixGuard)

target_sources(GLSMixGuard PRIVATE
    GLSMixGuardAudioProcessor.cpp
    GLSMixGuardAudioProcessor.h
)

target_link_libraries(GLSMixGuard PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
