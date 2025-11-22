juce_add_plugin(AEVGuerillaVerb
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.guerillaverb
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE AGVB
    FORMATS VST3
    PRODUCT_NAME "AEV Guerilla Verb"
)

juce_generate_juce_header(AEVGuerillaVerb)

target_sources(AEVGuerillaVerb PRIVATE
    AEVGuerillaVerbAudioProcessor.cpp
    AEVGuerillaVerbAudioProcessor.h
)

target_link_libraries(AEVGuerillaVerb PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
