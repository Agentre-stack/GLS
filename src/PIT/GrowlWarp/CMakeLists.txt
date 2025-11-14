juce_add_plugin(PITGrowlWarp
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.growlwarp
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE PGWp
    FORMATS VST3
    PRODUCT_NAME "PIT Growl Warp"
)

juce_generate_juce_header(PITGrowlWarp)

target_sources(PITGrowlWarp PRIVATE
    PITGrowlWarpAudioProcessor.cpp
    PITGrowlWarpAudioProcessor.h
)

target_link_libraries(PITGrowlWarp PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
