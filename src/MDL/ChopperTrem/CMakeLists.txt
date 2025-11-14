juce_add_plugin(MDLChopperTrem
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.choppertrem
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MCTR
    FORMATS VST3
    PRODUCT_NAME "MDL Chopper Trem"
)

juce_generate_juce_header(MDLChopperTrem)

target_sources(MDLChopperTrem
    PRIVATE
        MDLChopperTremAudioProcessor.cpp
        MDLChopperTremAudioProcessor.h
)

target_link_libraries(MDLChopperTrem
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
)
