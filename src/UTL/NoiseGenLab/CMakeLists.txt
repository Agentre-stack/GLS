juce_add_plugin(UTLNoiseGenLab
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.noisegenlab
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE UNGL
    FORMATS VST3
    PRODUCT_NAME "UTL NoiseGenLab"
)

juce_generate_juce_header(UTLNoiseGenLab)

target_sources(UTLNoiseGenLab PRIVATE
    UTLNoiseGenLabAudioProcessor.cpp
    UTLNoiseGenLabAudioProcessor.h
)

target_link_libraries(UTLNoiseGenLab PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
