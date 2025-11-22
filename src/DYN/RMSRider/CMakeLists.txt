juce_add_plugin(DYNRMSRider
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynrmsrider
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DRMZ
    FORMATS VST3
    PRODUCT_NAME "DYN RMS Rider"
)

juce_generate_juce_header(DYNRMSRider)

target_sources(DYNRMSRider PRIVATE
    DYNRMSRiderAudioProcessor.cpp
    DYNRMSRiderAudioProcessor.h
)

target_link_libraries(DYNRMSRider PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
