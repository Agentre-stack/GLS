juce_add_plugin(EQVoxDesignerEQ
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.eqvoxdesignereq
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE EVDE
    FORMATS VST3
    PRODUCT_NAME "EQ Vox Designer EQ"
)

juce_generate_juce_header(EQVoxDesignerEQ)

target_sources(EQVoxDesignerEQ PRIVATE
    EQVoxDesignerEQAudioProcessor.cpp
    EQVoxDesignerEQAudioProcessor.h
)

target_link_libraries(EQVoxDesignerEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
