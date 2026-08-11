# Design

The UI uses a shared monochrome 8-bit system: 8 px grid, grayscale palette, procedural horizontal pixel controls and motif, no external images, and no external fonts. The editor default size is 960 x 544 and the minimum is 720 x 432. `GenericAudioProcessorEditor` is banned for DHN9 products.

BitRash exposes every parameter as an APVTS parameter and as a custom editor control with a stable component ID, accessible name, keyboard focus, and tooltip. The visual language is a monochrome bit-grid/raster motif rather than a generic host editor.

The audio callback owns no file, network, logging, lock, or heap allocation work in steady state. `BitRashDSP` sanitizes non-finite input, smooths continuous targets, applies mode and seed changes at the next block boundary, applies discrete bit/deci clock choices deterministically, caps error-feedback/noise-shaping paths, and clamps output finite. A live seed change reseeds both channel RNGs and restarts pending hold clocks while preserving filter/error memories.
