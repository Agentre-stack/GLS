juce_add_plugin(PITTimeStack
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.timestack
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE PTSt
    FORMATS VST3
    PRODUCT_NAME "PIT Time Stack"
)

juce_generate_juce_header(PITTimeStack)

target_sources(PITTimeStack PRIVATE
    PITTimeStackAudioProcessor.cpp
    PITTimeStackAudioProcessor.h
)

target_link_libraries(PITTimeStack PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
