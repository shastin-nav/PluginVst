# Environment Audit (2026-11-04)

Performed before any code was written, as required by the engineering directive.

## Development container

| Item | Finding |
|---|---|
| OS | Ubuntu 24.04.5 LTS, x86_64, kernel 6.18 |
| CPU / RAM / disk | 4 cores, 15 GiB RAM, ~31 GB free |
| CMake | 3.28.3 (project minimum: 3.22) |
| Build tools | Ninja 1.11.1, GNU Make 4.3 |
| Compilers | GCC 13.3.0, Clang 18.1.3 (with lld and the LLVM gold plugin) |
| Git / Python | 2.43.0 / 3.13 |
| Display | none; Xvfb available for GUI tests |
| Network | GitHub and Ubuntu apt reachable; juce.com legal pages returned empty responses to automated requests |
| Repository | empty, branch `claude/echoform-stereo-effects-o4a6q4`, no commits |

Installed during the audit (apt): JUCE's Linux build dependencies (FreeType,
Fontconfig, X11 extension headers, Mesa/GLU headers, ALSA/JACK headers,
libcurl headers). The project's verified minimal list is in `docs/BUILDING.md`.

## Not available in the container

- macOS (Xcode, AU, `auval`) and Windows (MSVC). These platforms are built
  and validated only on GitHub Actions runners (`macos-15`, `windows-2022`).
- A commercial DAW. Host validation uses pluginval (a JUCE-based host that
  exercises loading, editor, processing, state, automation and buses) and
  Steinberg's own VST3 validator (a non-JUCE host).

## Dependency selection

| Dependency | Candidates seen | Chosen | Reason |
|---|---|---|---|
| JUCE | 8.0.x up to `8.0.17-security`; 9.0.0 to 9.0.5 also exist | `8.0.17-security` | Directive requires JUCE 8; newest JUCE 8 release, containing security hardening for XML/ValueTree/ZIP/image parsing (relevant to state loading) |
| GoogleTest | up to v1.17.0 | v1.17.0 | Latest release; C++17 compatible |
| pluginval | v1.0.4 stable; v2.0.0 betas | v1.0.4 | Latest non-beta release |
| VST3 SDK (validator) | up to v3.8.1 | v3.8.0_build_66 | Matches the VST3 SDK bundled in the pinned JUCE |

See `docs/DEPENDENCIES_AND_LICENSING.md` for pins, hashes and licences.
