# Warning policy for ECHOFORM's own code.
#
# JUCE module sources are compiled into the same targets, so strict flags are
# applied per source file (our files only) rather than per target. JUCE headers
# are treated as external/system headers so that our stricter flags (e.g.
# -Wold-style-cast) only report problems in ECHOFORM code.

set(ECHOFORM_GNU_WARNINGS
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wdouble-promotion)
set(ECHOFORM_MSVC_WARNINGS /W4 /permissive- /external:anglebrackets /external:W0)

function(echoform_warning_flags out_var)
    if(MSVC)
        set(flags ${ECHOFORM_MSVC_WARNINGS})
        if(ECHOFORM_WARNINGS_AS_ERRORS)
            list(APPEND flags /WX)
        endif()
    else()
        set(flags ${ECHOFORM_GNU_WARNINGS})
        if(DEFINED ECHOFORM_JUCE_DIR)
            # A directory given with both -I and -isystem is searched as a system directory.
            list(APPEND flags -isystem "${ECHOFORM_JUCE_DIR}/modules")
        endif()
        if(ECHOFORM_WARNINGS_AS_ERRORS)
            list(APPEND flags -Werror)
        endif()
    endif()
    set(${out_var} ${flags} PARENT_SCOPE)
endfunction()

# Apply strict warnings to every source of a target that is entirely ours.
function(echoform_apply_target_warnings target)
    echoform_warning_flags(flags)
    target_compile_options(${target} PRIVATE ${flags})
endfunction()

# Apply strict warnings to selected sources of a mixed (ours + JUCE) target.
function(echoform_apply_warning_policy target)
    echoform_warning_flags(flags)
    set(quoted "")
    foreach(flag IN LISTS flags)
        if(flag MATCHES " ")
            string(APPEND quoted " \"${flag}\"")
        else()
            string(APPEND quoted " ${flag}")
        endif()
    endforeach()
    foreach(src IN LISTS ARGN)
        set_property(SOURCE ${src} TARGET_DIRECTORY ${target} APPEND_STRING
            PROPERTY COMPILE_FLAGS "${quoted}")
    endforeach()
endfunction()
