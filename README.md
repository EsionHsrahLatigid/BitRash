# BitRash

BitRash is a deterministic 1-16 bit crusher with decimation, dither, and bounded error feedback. It combines 1-16 bit uniform quantization, sample-hold decimation, seeded TPDF dither, bounded error feedback/noise shaping, deterministic clock jitter, pre/post filtering, input clamp/fold/wrap modes, wet/dry mix, and output trim.

## Identity

- Product: `BitRash`
- Repository slug: `bitrash`
- Bundle ID: `jp.ehl.bitrash`
- Manufacturer: `EsionHsrahLatigid`
- Manufacturer code: `EHL_`
- Plugin code: `BtRh`

## Build

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release -DEHL_JUCE_SOURCE_DIR=/path/to/JUCE
cmake --build --preset plugin-release --target ehl_stage_products
ctest --preset plugin-release --output-on-failure
```

The rendered scaffold pins JUCE to `91ad83ae34a81e0833b1a2b0866f54846370ae53` when network FetchContent is used. Set `EHL_JUCE_SOURCE_DIR` for offline builds.

On local macOS builds outside CI, VST3 and AU products are also copied to the current user's standard plug-in folders. Override this with `-DEHL_COPY_PLUGIN_AFTER_BUILD=ON|OFF`. Standalone products are not copied to `Audio/Plug-Ins`; they remain in the build and artifact trees.

Stable artifacts:

```text
artifacts/plugin-release/macos-arm64/standalone/bitrash_standalone_plugin.app
artifacts/plugin-release/macos-arm64/vst3/bitrash_vst3_plugin.vst3
artifacts/plugin-release/macos-arm64/au/bitrash_au_plugin.component
artifacts/plugin-release/macos-arm64/ARTIFACTS.txt

artifacts/plugin-release/windows-x64/standalone/bitrash_standalone_plugin.exe
artifacts/plugin-release/windows-x64/vst3/bitrash_vst3_plugin.vst3
artifacts/plugin-release/windows-x64/ARTIFACTS.txt
```

## Tests

Targets are fixed for CI and humans:

- `bitrash_dsp_tests`
- `bitrash_plugin_tests`
- `bitrash_editor_tests`
- `ehl_stage_products`

## DSP

`Source/dsp/BitRashDSP.*` is JUCE-independent and preallocates all per-channel state at prepare/reset time. The steady-state process path performs no heap allocation, locks, file/network I/O, logging, or unbounded loops. Non-finite input and parameters are sanitized, feedback paths are capped, and final sample output is finite-clamped.

Discrete input mode changes (`Clamp`, `Fold`, `Wrap`) apply at the next audio block boundary. Seed changes also apply at the next audio block boundary: both channel RNG states are deterministically reseeded and pending sample-hold clocks are restarted without allocation; filter and error memories are otherwise preserved.

The dither and noise-shaping controls are deliberate harsh-noise tools. They are based on Lipshitz/Wannamaker/Vanderkooy quantization-error concepts, but BitRash does not claim transparent mastering-grade dither or formal psychoacoustic noise-shaping performance.
