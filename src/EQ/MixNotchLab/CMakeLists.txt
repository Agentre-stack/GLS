juce_add_plugin(EQMixNotchLab
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqmixnotchlab
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EMNL
    FORMATS VST3
    PRODUCT_NAME "EQ Mix Notch Lab"
)

juce_generate_juce_header(EQMixNotchLab)

target_sources(EQMixNotchLab PRIVATE
    EQMixNotchLabAudioProcessor.cpp
    EQMixNotchLabAudioProcessor.h
)

target_link_libraries(EQMixNotchLab PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
