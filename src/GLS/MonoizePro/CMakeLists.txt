juce_add_plugin(GLSMonoizePro
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.monoizepro
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GMPr
    FORMATS VST3
    PRODUCT_NAME "GLS Monoize Pro"
)

juce_generate_juce_header(GLSMonoizePro)

target_sources(GLSMonoizePro PRIVATE
    GLSMonoizeProAudioProcessor.cpp
    GLSMonoizeProAudioProcessor.h
)

target_link_libraries(GLSMonoizePro PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
