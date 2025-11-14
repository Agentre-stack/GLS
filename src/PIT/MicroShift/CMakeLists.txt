juce_add_plugin(PITMicroShift
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.microshift
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE PMSh
    FORMATS VST3
    PRODUCT_NAME "PIT Micro Shift"
)

target_sources(PITMicroShift PRIVATE
    PITMicroShiftAudioProcessor.cpp
    PITMicroShiftAudioProcessor.h
)

target_link_libraries(PITMicroShift PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
