juce_add_plugin(UTLLatencyLab
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.latencylab
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE ULLb
    FORMATS VST3
    PRODUCT_NAME "UTL Latency Lab"
)

juce_generate_juce_header(UTLLatencyLab)

target_sources(UTLLatencyLab PRIVATE
    UTLLatencyLabAudioProcessor.cpp
    UTLLatencyLabAudioProcessor.h
)

target_link_libraries(UTLLatencyLab PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
