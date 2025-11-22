juce_add_plugin(EQHarmonicEQ
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqharmoniceq
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EHME
    FORMATS VST3
    PRODUCT_NAME "EQ Harmonic EQ"
)

juce_generate_juce_header(EQHarmonicEQ)

target_sources(EQHarmonicEQ PRIVATE
    EQHarmonicEQAudioProcessor.cpp
    EQHarmonicEQAudioProcessor.h
)

target_link_libraries(EQHarmonicEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
