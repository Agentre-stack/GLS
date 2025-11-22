juce_add_plugin(GRDFaultLineFuzz
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.faultlinefuzz
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GFLF
    FORMATS VST3
    PRODUCT_NAME "GRD Fault Line Fuzz"
)

juce_generate_juce_header(GRDFaultLineFuzz)

target_sources(GRDFaultLineFuzz PRIVATE
    GRDFaultLineFuzzAudioProcessor.cpp
    GRDFaultLineFuzzAudioProcessor.h
)

target_link_libraries(GRDFaultLineFuzz PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
