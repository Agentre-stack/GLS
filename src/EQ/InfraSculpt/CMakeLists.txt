juce_add_plugin(EQInfraSculpt
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqinfrasculpt
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EISC
    FORMATS VST3
    PRODUCT_NAME "EQ Infra Sculpt"
)

juce_generate_juce_header(EQInfraSculpt)

target_sources(EQInfraSculpt PRIVATE
    EQInfraSculptAudioProcessor.cpp
    EQInfraSculptAudioProcessor.h
)

target_link_libraries(EQInfraSculpt PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
