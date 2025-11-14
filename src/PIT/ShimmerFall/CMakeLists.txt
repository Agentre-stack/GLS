juce_add_plugin(PITShimmerFall
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.shimmerfall
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE PSFl
    FORMATS VST3
    PRODUCT_NAME "PIT Shimmer Fall"
)

juce_generate_juce_header(PITShimmerFall)

target_sources(PITShimmerFall PRIVATE
    PITShimmerFallAudioProcessor.cpp
    PITShimmerFallAudioProcessor.h
)

target_link_libraries(PITShimmerFall PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
