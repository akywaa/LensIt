# LensIt

A lightweight desktop screen magnifier and annotation tool for Windows. Inspired by Sysinternals ZoomIt, but designed around smooth magnification via the Magnification API (`MagSetFullscreenTransform`) and presentation overlays.

## Features

- Fullscreen cursor-centered magnification using Windows Magnification API
- Annotations: freehand pen, directional arrows, rectangles, highlighter, and step badges
- Privacy blur box for redacting sensitive screen areas
- Auto-fading laser ink and spotlight mode
- Built-in break countdown timer
- Pinned overlay mode (drawings stay on screen with click-through enabled)
- Screenshot and crop selection to clipboard

## Hotkeys

Default trigger is **Alt** (customizable in Settings):

| Hotkey | Action |
| --- | --- |
| `Alt` + Wheel | Zoom in / out |
| `Alt` + LMB drag | Draw line |
| `Alt` + RMB drag | Draw arrow |
| `Alt` + Shift + LMB drag | Draw rectangle |
| `Alt` + Shift + drag | Snap line / arrow to 0°, 45°, 90° |
| `Alt` + MMB click | Drop numbered step badge |
| `Alt` + `X` | On-screen text prompt (`Enter` to commit, `Esc` to cancel) |
| `Alt` + `H` | Toggle highlighter mode |
| `Alt` + `O` | Toggle blur / redaction box |
| `Alt` + `V` | Toggle laser ink (strokes fade out) |
| `Alt` + `S` | Toggle spotlight mode |
| `Alt` + `W` | Cycle whiteboard (White -> Dark -> Off) |
| `Alt` + `K` | Toggle keystroke HUD |
| `Alt` + `T` | Toggle break timer |
| `Alt` + `R` / `G` / `B` / `Y` | Quick color presets |
| `Alt` + `Z` | Undo last stroke |
| `Alt` + `C` | Copy fullscreen screenshot to clipboard |
| `Alt` + `Shift` + `C` | Crop region to clipboard |
| `Alt` + `P` | Pin drawings (click-through mode) |
| `Esc` | Clear / reset active mode |

### Break Timer Controls
- **Scroll** or **Up / Down**: adjust by 1 minute (hold **Shift** for 5 seconds).
- **Click timer**: enter minutes/seconds directly.
- **Space**: pause/resume.
- **Esc**: dismiss timer.

## Build Requirements

- Windows 10 / 11 SDK
- Visual Studio 2022 (MSVC v143 toolset) with C++17 support
- Built with standard Windows libraries (`magnification.lib`, `gdiplus.lib`, `dwmapi.lib`). No external dependencies.

Open `LensIt.vcxproj` and build in `Release | x64`.

## License

MIT