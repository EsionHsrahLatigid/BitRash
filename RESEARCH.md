# BitRash Research Map

## Sources

- Lipshitz, Wannamaker, and Vanderkooy, AES convention record `https://secure.aes.org/forum/pubs/conventions/?elib=5575`: primary basis for treating dither and error feedback/noise shaping as quantization-error controls.
- G001 DHN9 source register: maps BitRash to the Lipshitz/Wannamaker/Vanderkooy dither/noise-shaping basis and the local Noise project's deterministic harsh-noise safety posture.
- G002 reviewed foundation: supplies the JUCE/CMake/APVTS/custom-editor/staging structure, reviewed Windows VST3 directory staging, and matched mono/stereo bus contract.

## Source To Decision Map

- 1-16 bit quantizer: direct finite-word-length interpretation; tests verify level count and grid step.
- Sample-hold decimation: standard bitcrusher identity; tests verify exact hold-run lengths at fixed divisor.
- Seeded TPDF dither: uses two deterministic uniform PRNG draws per sample; tests verify deterministic reset and bounded zero-input statistics.
- Error feedback/noise shaping: bounded feedback of previous quantization error with a high-frequency proxy term. This is an intentionally audible DHN control, not a high-fidelity transparency claim.
- Jittered clock: deterministic seeded variation of hold length; tests verify identical seeded renders and difference from fixed divisor.
- Pre/post filters: one-pole filtering around the destructive stages; tests verify a measurable high-frequency reduction.

## Limits

BitRash intentionally aliases and exposes quantization error. It does not claim BS.1770, perceptual codec, mastering dither, or transparent noise-shaping compliance. Digital output is finite-bounded, but this is not an SPL or hearing-safety guarantee.
