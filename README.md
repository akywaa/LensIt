<div align="center">

<!-- App icon goes here -->
<img src="app.ico" width="96" height="96" alt="LensIt icon">

# LensIt

A lightweight screen magnifier and annotation overlay for Windows, inspired by Sysinternals ZoomIt, but with smoother zooming and a few extra presentation tools (laser ink, step badges, blur, timer).

Written in pure C++ (Win32 API + GDI+), no third-party dependencies.

---

## What it does

- **Cursor-centered Magnifier:** Uses the Windows Magnification API (`MagSetFullscreenTransform`) for hardware-accelerated zoom without lag.
- **On-screen Drawing:** Freehand lines, arrows, rectangles, and highlighters.
- **Step Badges:** Numbered markers (1, 2, 3...) to guide walkthroughs or tutorials.
- **Privacy Blur:** Quickly redact tokens, passwords, or credentials on screen.
- **Break Timer:** A fullscreen overlay with a countdown clock for talks, webinars, or study sessions.
- **Click-through Pin Mode:** Keeps annotations on screen while letting you interact with underlying windows.
- **Spotlight & Laser Ink:** Dim everything except the cursor area, or draw strokes that fade out automatically after a second.

---

## Controls

The default trigger key is **Alt**. Hold it down to access shortcuts:

| Action | Hotkey |
| --- | --- |
| **Zoom in / out** | `Alt` + Mouse Wheel |
| **Draw Line** | `Alt` + Left Click + Drag |
| **Draw Arrow** | `Alt` + Right Click + Drag |
| **Snap angle (0° / 45° / 90°)** | `Alt` + `Shift` + Drag |
| **Draw Rectangle** | `Alt` + `Shift` + Left Click |
| **Drop Step Badge (1, 2...)** | `Alt` + Middle Click |
| **Text on screen** | `Alt` + `X` (`Enter` to finish, `Esc` to cancel) |
| **Toggle Highlighter** | `Alt` + `H` |
| **Toggle Blur Redaction** | `Alt` + `O` |
| **Laser Ink (auto-fade)** | `Alt` + `V` |
| **Spotlight mode** | `Alt` + `S` |
| **Whiteboard / Blackboard** | `Alt` + `W` (cycles White -> Dark -> Off) |
| **Keystroke HUD** | `Alt` + `K` |
| **Break Timer** | `Alt` + `T` |
| **Color Switch** | `Alt` + `R` (Red) / `G` (Green) / `B` (Blue) / `Y` (Yellow) |
| **Undo last stroke** | `Alt` + `Z` |
| **Full Screenshot** | `Alt` + `C` |
| **Crop Screenshot** | `Alt` + `Shift` + `C` |
| **Pin drawings** | `Alt` + `P` (prevents clearing on release) |
| **Reset / Exit active tool** | `Esc` |

*The trigger key and rectangle modifier can be rebound in Settings (right-click the tray icon).*

### Break Timer Controls
- **Scroll Wheel** or `↑` / `↓` adjusts time by ±1 minute.
- **Shift + Scroll** or `Shift + ↑` / `↓` adjusts by ±5 seconds.
- **Click the clock** to type time directly (`3:50`, `10`, etc.).
- **Space** toggles pause, **Esc** dismisses the timer.

---

## Limitations

- **Exclusive Fullscreen:** LensIt uses a layered desktop window (`UpdateLayeredWindow`) combined with the Windows Magnification engine. It will not render over games running in true exclusive fullscreen. If you need it over a game, switch the game to **Borderless Windowed**.

---

## Building

Requires Visual Studio 2022+ with the **Desktop development with C++** workload.

1. Clone the repo:
   ```bash
   git clone https://github.com/akywaa/LensIt.git
   cd LensIt
   ```
2. Open `LensIt.vcxproj` in Visual Studio.
3. Select **Release / x64** and build (`Ctrl + Shift + B`).

All linked libraries (`magnification.lib`, `gdiplus.lib`, `dwmapi.lib`) are included in the default Windows SDK.

---

## License

MIT