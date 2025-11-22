juce_add_plugin(DYNSmoothDestroyer
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.smoothdestroyer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DSDy
    FORMATS VST3
    PRODUCT_NAME "DYN Smooth Destroyer"
)

juce_generate_juce_header(DYNSmoothDestroyer)

target_sources(DYNSmoothDestroyer PRIVATE
    DYNSmoothDestroyerAudioProcessor.cpp
    DYNSmoothDestroyerAudioProcessor.h
)

target_link_libraries(DYNSmoothDestroyer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
