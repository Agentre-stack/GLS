cmake_minimum_required(VERSION 3.15)
project(GLS_SUITE VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

add_subdirectory(JUCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/GLS/ChannelPilot ${CMAKE_BINARY_DIR}/ChannelPilot)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/GLS/ChannelStripOne ${CMAKE_BINARY_DIR}/ChannelStripOne)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/MDL/ChopperTrem ${CMAKE_BINARY_DIR}/ChopperTrem)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/GRD/MixHeat ${CMAKE_BINARY_DIR}/MixHeat)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/PIT/ShiftPrime ${CMAKE_BINARY_DIR}/ShiftPrime)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/PIT/DoubleStrike ${CMAKE_BINARY_DIR}/DoubleStrike)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/PIT/ShimmerFall ${CMAKE_BINARY_DIR}/ShimmerFall)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/PIT/GrowlWarp ${CMAKE_BINARY_DIR}/GrowlWarp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/UTL/SignalTracer ${CMAKE_BINARY_DIR}/SignalTracer)
