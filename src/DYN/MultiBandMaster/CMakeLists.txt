juce_add_plugin(DYNMultiBandMaster
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynmultibandmaster
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DMBM
    FORMATS VST3
    PRODUCT_NAME "DYN MultiBand Master"
)

juce_generate_juce_header(DYNMultiBandMaster)

target_sources(DYNMultiBandMaster PRIVATE
    DYNMultiBandMasterAudioProcessor.cpp
    DYNMultiBandMasterAudioProcessor.h
)

target_link_libraries(DYNMultiBandMaster PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
