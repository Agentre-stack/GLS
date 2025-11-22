juce_add_plugin(DYNBusLift
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.dynbuslift
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE DBLF
    FORMATS VST3
    PRODUCT_NAME "DYN Bus Lift"
)

juce_generate_juce_header(DYNBusLift)

target_sources(DYNBusLift PRIVATE
    DYNBusLiftAudioProcessor.cpp
    DYNBusLiftAudioProcessor.h
)

target_link_libraries(DYNBusLift PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
