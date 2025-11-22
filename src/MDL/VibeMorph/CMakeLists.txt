juce_add_plugin(MDLVibeMorph
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.vibemorph
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE MVMp
    FORMATS VST3
    PRODUCT_NAME "MDL Vibe Morph"
)

juce_generate_juce_header(MDLVibeMorph)

target_sources(MDLVibeMorph PRIVATE
    MDLVibeMorphAudioProcessor.cpp
    MDLVibeMorphAudioProcessor.h
)

target_link_libraries(MDLVibeMorph PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
