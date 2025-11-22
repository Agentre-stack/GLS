juce_add_plugin(GRDBiteShaper
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.biteshaper
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GBSp
    FORMATS VST3
    PRODUCT_NAME "GRD Bite Shaper"
)

juce_generate_juce_header(GRDBiteShaper)

target_sources(GRDBiteShaper PRIVATE
    GRDBiteShaperAudioProcessor.cpp
    GRDBiteShaperAudioProcessor.h
)

target_link_libraries(GRDBiteShaper PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
