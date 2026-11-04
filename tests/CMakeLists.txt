include(FetchContent)

# GoogleTest v1.17.0 (BSD-3-Clause), pinned by release archive hash.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# ---------------------------------------------------------------------------
# Core tests: framework-independent parameter model and DSP. No JUCE.
# ---------------------------------------------------------------------------
add_executable(EchoformCoreTests
    core/FloatSafetyTests.cpp
    core/GainTests.cpp
    core/LinearSmootherTests.cpp
    core/ParameterSpecTests.cpp)
target_link_libraries(EchoformCoreTests PRIVATE echoform_core GTest::gtest_main)
echoform_apply_test_warnings(EchoformCoreTests)
gtest_discover_tests(EchoformCoreTests DISCOVERY_MODE PRE_TEST)

# ---------------------------------------------------------------------------
# Plug-in tests: exercise the real AudioProcessor/editor.
#
# Links the plug-in's shared-code static library (which already contains the
# compiled JUCE modules) and inherits its complete, transitive set of include
# directories and definitions, so the tests are built with exactly the
# configuration of the shipped plug-in and JUCE is compiled only once.
# ---------------------------------------------------------------------------
add_executable(EchoformPluginTests
    plugin/AllocationTracker.cpp
    plugin/AllocationTracker.h
    plugin/EditorTests.cpp
    plugin/ParameterLayoutTests.cpp
    plugin/ProcessingTests.cpp
    plugin/RealtimeSafetyTests.cpp
    plugin/StateTests.cpp
    plugin/TestHelpers.h
    plugin/TestMain.cpp)
target_link_libraries(EchoformPluginTests PRIVATE Echoform echoform_core GTest::gtest)
target_include_directories(EchoformPluginTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    $<TARGET_PROPERTY:Echoform,INCLUDE_DIRECTORIES>)
target_compile_definitions(EchoformPluginTests PRIVATE
    $<TARGET_PROPERTY:Echoform,COMPILE_DEFINITIONS>)
echoform_apply_test_warnings(EchoformPluginTests)
gtest_discover_tests(EchoformPluginTests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60)
