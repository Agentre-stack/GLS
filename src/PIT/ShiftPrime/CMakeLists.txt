juce_add_plugin(PITShiftPrime
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.shiftprime
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE PSPr
    FORMATS VST3
    PRODUCT_NAME "PIT Shift Prime"
)

juce_generate_juce_header(PITShiftPrime)

target_sources(PITShiftPrime PRIVATE
    PITShiftPrimeAudioProcessor.cpp
    PITShiftPrimeAudioProcessor.h
)

target_link_libraries(PITShiftPrime PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
