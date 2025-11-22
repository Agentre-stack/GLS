juce_add_plugin(MDLDualTap
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dualtap
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MDTp
    FORMATS VST3
    PRODUCT_NAME "MDL Dual Tap"
)

juce_generate_juce_header(MDLDualTap)

target_sources(MDLDualTap PRIVATE
    MDLDualTapAudioProcessor.cpp
    MDLDualTapAudioProcessor.h
)

target_link_libraries(MDLDualTap PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
