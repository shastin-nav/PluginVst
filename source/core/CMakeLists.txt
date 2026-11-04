# echoform_core: framework-independent C++17 code.
#
# Contains the parameter model (single source of truth for every parameter's
# ID, range and default) and the DSP building blocks. It must never depend on
# JUCE, so that it can be unit tested in isolation and the GUI can never reach
# into DSP implementations through it.
#
# Currently header-only; it becomes a STATIC library once DSP engines with
# out-of-line implementations are added.

add_library(echoform_core INTERFACE)
add_library(echoform::core ALIAS echoform_core)

target_include_directories(echoform_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(echoform_core INTERFACE cxx_std_17)
