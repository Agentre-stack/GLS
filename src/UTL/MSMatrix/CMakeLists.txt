juce_add_plugin(UTLMSMatrix
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.msmatrix
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE UMMx
    FORMATS VST3
    PRODUCT_NAME "UTL MS Matrix"
)

juce_generate_juce_header(UTLMSMatrix)

target_sources(UTLMSMatrix PRIVATE
    UTLMSMatrixAudioProcessor.cpp
    UTLMSMatrixAudioProcessor.h
)

target_link_libraries(UTLMSMatrix PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
