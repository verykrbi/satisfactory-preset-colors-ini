# Preset Colors INI

A Satisfactory mod (SML plugin) that adds your own named colors to the color picker presets. The colors are read from an `.ini` file when a game starts.

## Usage

1. Install the mod and start the game once. It creates an example file at:

   `%LOCALAPPDATA%\FactoryGame\Saved\PresetColorsIni.ini`

2. Edit the file and add your colors under `[Colors]`:

   ```ini
   [Colors]
   Sunset Orange=#FF7A1A
   Deep Teal=0,128,128
   ```

3. Restart the game, or reload your save. The colors appear in the color picker presets.

A complete example, with 32 colors, is in [Examples/PresetColorsIni.ini](Examples/PresetColorsIni.ini).

### Format

| Form | Example | Notes |
| --- | --- | --- |
| `Name=#RRGGBB` | `Sunset Orange=#FF7A1A` | Hex, as shown in the color picker. The `#` is optional. |
| `Name=R,G,B` | `Deep Teal=0,128,128` | Each channel is an integer from 0 to 255. |
| `Name=linear(R,G,B)` | `Warm Lamp=linear(1.0,0.6,0.2)` | The game's own values: linear floats where 1.0 is white, used as is. A 4th value is alpha (default 1). |
| `Name=hsv(H,S,V)` | `Super White=hsv(0,0,10)` | Hue 0-360, saturation 0-1, value 0 or more. A value above 1 is brighter than white. Used as is, not converted. |

- Hex and `R,G,B` colors are treated as sRGB and converted to linear. Colors that need brightness above white have to use `hsv(...)`.
- The export writes every saved preset as `linear(R,G,B,A)`, the exact values the game stores, with no conversion.
- Lines starting with `;` or `#` are comments, and blank lines are ignored.
- Only the `[Colors]` section is read.
- Invalid lines are skipped, and a warning is written to the log (category `LogPresetColorsIni`).

## Colors that are already in your save

If a color from `PresetColorsIni.ini` is already one of the presets you saved in the color picker, it isn't added a second time. Instead, your saved preset moves to the top of your saved list, following the order of the file. The other saved presets keep their order below.

- Colors are matched by value, not by name. Ordinary colors compare as 8-bit sRGB, so a hex entry matches a saved color that rounds to it. Colors brighter than white compare as floats.
- The saved list is part of your save, so the new order is stored the next time you save. Only the host changes it.

## Exporting the colors from your save

Whenever a world starts, the presets you saved in the color picker are written to `%LOCALAPPDATA%\FactoryGame\Saved\PresetColorsIni.Exported.ini` in the same format. Copy the lines you want into `PresetColorsIni.ini`. The file is overwritten each time, and nothing is written if the save has no saved presets.

## Notes

- Your colors are placed at the top of the preset list, in the order they appear in the file.
- The colors are added to the game's static presets, so they show up in every save. Presets that players save themselves are not touched.
- The presets are not replicated. Each player (and each dedicated server) reads its own `.ini`.
- The colors are added when the game state starts (a hook on `AFGGameState::BeginPlay`). The hook is disabled in the editor, so test in the packaged game.
- `mStaticGlobalColorPresets` is private in the game's code, so the mod finds it by name. If a game update renames it, the log shows an error and no colors are added.

## Building

Requires the Satisfactory modding project and SML ^3.12.0. Package with Alpakit, or with `RunUAT PackagePlugin` (including `-build -nocompileeditor`, otherwise no Shipping DLL is built).

## License

MIT. See [LICENSE](LICENSE).

## Releasing

Pushing a version tag (for example `v0.1.0`) builds the mod on a self-hosted runner and publishes a GitHub Release. See [.github/ci/README.md](.github/ci/README.md).

## Layout

- `PresetColorsIni.uplugin`: plugin descriptor.
- `Source/PresetColorsIni/`: the C++ module.
  - `Private/PresetColorsIni.cpp`: parses the `.ini` and patches the presets.
