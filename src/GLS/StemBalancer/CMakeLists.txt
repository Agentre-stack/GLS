juce_add_plugin(GLSStemBalancer
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.stembalancer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GSBl
    FORMATS VST3
    PRODUCT_NAME "GLS Stem Balancer"
)

juce_generate_juce_header(GLSStemBalancer)

target_sources(GLSStemBalancer PRIVATE
    GLSStemBalancerAudioProcessor.cpp
    GLSStemBalancerAudioProcessor.h
)

target_link_libraries(GLSStemBalancer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
