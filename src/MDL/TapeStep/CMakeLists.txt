juce_add_plugin(MDLTapeStep
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.tapestep
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MTSt
    FORMATS VST3
    PRODUCT_NAME "MDL Tape Step"
)

juce_generate_juce_header(MDLTapeStep)

target_sources(MDLTapeStep PRIVATE
    MDLTapeStepAudioProcessor.cpp
    MDLTapeStepAudioProcessor.h
)

target_link_libraries(MDLTapeStep PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
