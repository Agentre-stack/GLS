juce_add_plugin(GRDBassMaul
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.bassmaul
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GBMl
    FORMATS VST3
    PRODUCT_NAME "GRD Bass Maul"
)

juce_generate_juce_header(GRDBassMaul)

target_sources(GRDBassMaul PRIVATE
    GRDBassMaulAudioProcessor.cpp
    GRDBassMaulAudioProcessor.h
)

target_link_libraries(GRDBassMaul PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
