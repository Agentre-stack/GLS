juce_add_plugin(GLSParallelPress
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.parallelpress
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE GPPr
    FORMATS VST3
    PRODUCT_NAME "GLS Parallel Press"
)

juce_generate_juce_header(GLSParallelPress)

target_sources(GLSParallelPress PRIVATE
    GLSParallelPressAudioProcessor.cpp
    GLSParallelPressAudioProcessor.h
)

target_link_libraries(GLSParallelPress PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
