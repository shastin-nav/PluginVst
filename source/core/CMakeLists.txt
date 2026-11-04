# echoform_core: framework-independent C++17 code.
#
# Contains the parameter model (single source of truth for every parameter's
# ID, range and default), the DSP building blocks, the Delay and Reverb
# engines and the routing/mixing chain. It must never depend on JUCE, so that
# it can be unit tested in isolation and the GUI can never reach into DSP
# implementations through it.

add_library(echoform_core STATIC
    engines/DelayEngine.cpp
    engines/EffectChain.cpp
    engines/FdnReverb.cpp
    engines/ParameterMapping.cpp)
add_library(echoform::core ALIAS echoform_core)

target_include_directories(echoform_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(echoform_core PUBLIC cxx_std_17)

# Linked into the plug-in's shared module.
set_target_properties(echoform_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

echoform_apply_target_warnings(echoform_core)
