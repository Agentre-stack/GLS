juce_add_plugin(MDLWideTrack
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.widetrack
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MWTk
    FORMATS VST3
    PRODUCT_NAME "MDL Wide Track"
)

juce_generate_juce_header(MDLWideTrack)

target_sources(MDLWideTrack PRIVATE
    MDLWideTrackAudioProcessor.cpp
    MDLWideTrackAudioProcessor.h
)

target_link_libraries(MDLWideTrack PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
