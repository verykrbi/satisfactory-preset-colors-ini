# Changelog

All notable changes to this mod are listed here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [0.1.1] - 2026-11-19

### Fixed

- The plugin's `Version` field now matches the major number of `SemVersion`, which ficsit.app requires. Version 0.1.0 was rejected there.

## [0.1.0] - 2026-11-18

First release.

### Added

- Your own named colors are read from `%LOCALAPPDATA%\FactoryGame\Saved\PresetColorsIni.ini` and added to the top of the color picker presets, in file order.
- Color formats: `#RRGGBB`, `R,G,B`, `hsv(H,S,V)` and `linear(R,G,B[,A])`. HSV and linear values can be brighter than white.
- Presets already saved in your save are matched by value and moved to follow the file order, instead of being added a second time (host only).
- Every time a world starts, the presets saved in your save are exported to `PresetColorsIni.Exported.ini`.
- An example file is created on the first run.

[0.1.1]: https://github.com/verykrbi/satisfactory-preset-colors-ini/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/verykrbi/satisfactory-preset-colors-ini/releases/tag/v0.1.0
