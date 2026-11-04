# Dependencies and Licensing

Last verified: 2026-11-04. Every version below is pinned; every licence below
was read from the dependency's own licence file at the pinned version unless
stated otherwise.

## Summary

| Dependency | Version / pin | Licence | Role | Shipped in ECHOFORM binaries |
|---|---|---|---|---|
| JUCE | tag `8.0.17-security`, commit `dabd5ad13cd869cba41046947fffc8e5ce5cd662` (git submodule `libs/JUCE`) | AGPLv3 **or** commercial JUCE 8 licence (dual) | Framework, plug-in wrappers | **Yes** |
| VST3 SDK (bundled in JUCE) | 3.8.0 | MIT | VST3 wrapper | Yes |
| AudioUnitSDK (bundled in JUCE) | as bundled in JUCE 8.0.17 | Apache-2.0 | AU wrapper (macOS) | Yes (AU only) |
| Other JUCE-bundled code compiled into the plug-in (zlib, pnglib, jpglib, HarfBuzz, SheenBidi) | as bundled | zlib, zlib, IJG, "Old MIT", Apache-2.0 respectively (per JUCE `LICENSE.md`) | Inside `juce_core` / `juce_graphics` | Yes |
| GoogleTest | v1.17.0, release archive SHA-256 `65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c` | BSD-3-Clause | Unit tests | No |
| pluginval | v1.0.4 release binaries, SHA-256 per platform in `scripts/fetch_pluginval.sh` | GPLv3 | Validation tool (separate process) | No |
| Steinberg VST3 validator | VST3 SDK `v3.8.0_build_66`, commit `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0` | MIT | Validation tool (separate process) | No |
| actions/checkout | v7.0.1, commit `3d3c42e5aac5ba805825da76410c181273ba90b1` | MIT | CI only | No |
| actions/upload-artifact | v7.0.2, commit `cf430e030ddbb5b0abf93d22962f4752f3646cd9` | MIT | CI only | No |

No other third-party code is used. JUCE is built without the web browser
component and without libcurl (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`), so
WebKit/GTK and libcurl are neither required nor linked.

## JUCE: what was verified, and what was not

### Verified (from `libs/JUCE/LICENSE.md` at the pinned commit)

> The JUCE Framework modules are dual-licensed under the AGPLv3 and the
> commercial JUCE licence.

> If you are not licensing the JUCE Framework modules under the AGPLv3 then by
> downloading, installing, or using the JUCE Framework, or combining the JUCE
> Framework with any other source code, object code, content or any other
> copyrightable work, you agree to the terms of the JUCE 8 End User Licence
> Agreement [...] which will bind you.

Also verified in the source: this JUCE version has **no splash screen**. The
`JUCE_DISPLAY_SPLASH_SCREEN` flag is ignored with a compiler warning
(`modules/juce_gui_basics/juce_gui_basics.cpp`).

### Not verified

The JUCE 8 End User Licence Agreement itself (`https://juce.com/legal/juce-8-licence/`)
and the pricing/tier page are hosted on juce.com, not in the repository. Requests
to those pages from the development environment returned HTTP 202 with an empty
body, so their text could not be read. Consequently, **this project makes no
claim** about:

- which commercial licence tiers exist, their revenue or funding limits, or prices;
- whether any free tier permits closed-source commercial distribution;
- whether an educational or student licence exists, who qualifies, or whether it
  permits selling a product. No student or educational exemption is assumed.

### Consequences for distribution

ECHOFORM statically links JUCE into every plug-in binary. Before **any**
distribution of binaries (including free betas, and including CI artefacts if
the repository is public), the product owner must choose one of:

1. **AGPLv3.** ECHOFORM's complete corresponding source must be made available
   to recipients under AGPLv3-compatible terms. Selling binaries is possible,
   but the source cannot be kept closed.
2. **Commercial JUCE licence.** Read the JUCE 8 EULA directly at
   <https://juce.com/legal/juce-8-licence/>, confirm which licence applies to
   the business and its revenue, and obtain it before release.

This decision is tracked as an open item in `PROJECT_STATUS.md`. Until it is
made, built binaries are for internal development and testing only. (As of
2026-11-04 the GitHub repository is private, so CI artefacts are visible only
to people with access to it. Making the repository public would change that.)

## Other obligations for a release (tracked for milestone P3)

- **Third-party notices.** The MIT, BSD, zlib, IJG, Apache-2.0 and "Old MIT"
  licences of code compiled into the binaries require their notices to be
  reproduced in the distribution (e.g. an "Acknowledgements" file in the
  installer). Apache-2.0 additionally requires any NOTICE file to be included.
- **VST trademark.** The VST3 SDK ships `VST3_Usage_Guidelines.pdf`
  (Steinberg's rules for using the VST name and logo). It has not yet been
  reviewed; it must be before any marketing material uses the VST mark.
- **Plug-in identity.** Company name, bundle ID and the 4-character
  manufacturer/plug-in codes in `CMakeLists.txt` are provisional and must be
  finalised before the first public release (hosts store them in sessions).
- **Code signing / notarisation** (macOS) and signing (Windows) are not yet set up.

## Test-only tools

pluginval (GPLv3) and the Steinberg validator (MIT) are downloaded/built into
the git-ignored `tools/` directory and run as separate processes against the
built plug-in. They are never linked into or distributed with ECHOFORM.
GoogleTest is linked only into the test executables.
