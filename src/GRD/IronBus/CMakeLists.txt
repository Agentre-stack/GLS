juce_add_plugin(GRDIronBus
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.ironbus
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GIBs
    FORMATS VST3
    PRODUCT_NAME "GRD Iron Bus"
)

juce_generate_juce_header(GRDIronBus)

target_sources(GRDIronBus PRIVATE
    GRDIronBusAudioProcessor.cpp
    GRDIronBusAudioProcessor.h
)

target_link_libraries(GRDIronBus PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
