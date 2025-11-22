juce_add_plugin(GRDTopFizz
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.topfizz
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GTfz
    FORMATS VST3
    PRODUCT_NAME "GRD Top Fizz"
)

juce_generate_juce_header(GRDTopFizz)

target_sources(GRDTopFizz PRIVATE
    GRDTopFizzAudioProcessor.cpp
    GRDTopFizzAudioProcessor.h
)

target_link_libraries(GRDTopFizz PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
