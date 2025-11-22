juce_add_plugin(GRDBitSpear
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.bitspear
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GBSr
    FORMATS VST3
    PRODUCT_NAME "GRD Bit Spear"
)

juce_generate_juce_header(GRDBitSpear)

target_sources(GRDBitSpear PRIVATE
    GRDBitSpearAudioProcessor.cpp
    GRDBitSpearAudioProcessor.h
)

target_link_libraries(GRDBitSpear PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
