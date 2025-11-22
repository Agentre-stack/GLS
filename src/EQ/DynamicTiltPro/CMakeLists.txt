juce_add_plugin(EQDynamicTiltPro
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqdynamictiltpro
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EDTL
    FORMATS VST3
    PRODUCT_NAME "EQ Dynamic Tilt Pro"
)

juce_generate_juce_header(EQDynamicTiltPro)

target_sources(EQDynamicTiltPro PRIVATE
    EQDynamicTiltProAudioProcessor.cpp
    EQDynamicTiltProAudioProcessor.h
)

target_link_libraries(EQDynamicTiltPro PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
