juce_add_plugin(UTLSignalTracer
    COMPANY_NAME "Goodluck Soundworks"
    BUNDLE_ID com.goodlucksoundworks.signaltracer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE USTr
    FORMATS VST3
    PRODUCT_NAME "UTL Signal Tracer"
)

juce_generate_juce_header(UTLSignalTracer)

target_sources(UTLSignalTracer PRIVATE
    UTLSignalTracerAudioProcessor.cpp
    UTLSignalTracerAudioProcessor.h
)

target_link_libraries(UTLSignalTracer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)
