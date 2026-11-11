# Touch controls (Android)

On-screen controls for the Android port: analog stick, A / B, C buttons, Z / L / R, Start, optional D-Pad, camera look by
dragging, optional phone gyro aiming, vibration, a left-handed layout, a floating stick and a drag-to-arrange layout editor.

Everything here is wrapped in `#ifdef __ANDROID__`, so it compiles to nothing on other platforms. The game's CMake picks the
files up on its own (it globs `soh/Enhancements/**`), and **nothing inside the `libultraship` submodule is modified**.

## How it plugs in

Three small hooks in game code, all Android-only:

| Where | What |
| --- | --- |
| `src/code/padmgr.c` | right after the pad is read, `TouchControls_ReadToPad(&pads[0])` merges the controls into player 1 |
| `soh/SohGui.cpp` | registers `TouchOverlayWindow`, an always-visible GUI window whose only job is to call `TouchControls::Draw()` every frame |
| `soh/SohMenuBar.cpp` | a "Touch Controls" button in Settings -> Controller opens the settings window |

The touch screen is presented to the game as one more libultraship `Controller` (`TouchController`), so it shares the
engine's existing pad merging (stick processing, simulated input lag, ...). Buttons are written straight into N64 button
bits, so the C buttons are independent of the right stick, which is free for camera look.

## Files

| File | Responsibility |
| --- | --- |
| `TouchCore.{h,cpp}` | **Pure logic, no SDL / ImGui / game dependencies.** Default layout, hit-testing, which finger does what (stick, buttons, look, drag), and `MapLook`, the drag -> right stick / gyro mapping. Unit-tested on a desktop. |
| `TouchSettings.{h,cpp}` | The player's options and layout, persisted as `gTouch.*` console variables. First-run game defaults. |
| `TouchOverlay.{h,cpp}` | Everything drawn with ImGui: the controls (glyphs are drawn as strokes, so they are crisp and need no font), the layout-editor toolbar, the settings widgets. |
| `TouchControls.{h,cpp}` | The orchestrator: reads fingers from SDL once per frame, picks the mode, runs the engine, feeds the game once per tick, phone gyro, vibration. |
| `TouchController.{h,cpp}` | The `LUS::Controller` adapter. |
| `TouchOverlayWindow.h`, `TouchControlsBridge.h` | The GUI window that drives the per-frame update, and the C entry point for `padmgr.c`. |

Two clocks are involved. The overlay updates once per **rendered frame** (smooth visuals, low latency) and the game reads
once per **game tick** (20 Hz). The engine accumulates the camera drag in between and hands it over when the game asks.

## How the camera look works

A drag is measured in *screen heights*, so it feels the same on every phone. `MapLook` turns it into two outputs, because
the game reads different inputs in different situations:

* **Right stick**: orbits the camera in third person (needs the game's *Free camera*; the stick is a rate, so finger *speed* counts).
* **Gyro channels**: turn the view while aiming in first person (an amount, so the drag *distance* counts).

The game only reads each in its own situation, so feeding both never double-turns. Dragging right looks right and dragging up
looks up whatever the player picked for the game's own camera-invert options; those are compensated for. The exception is the
game's *Right stick aiming* option, which makes first person read the right stick too, so the gyro channels stay silent then.
The sign conventions were derived from `z_camera.c` / `z_player.c` and are pinned by `tests/touch-controls`.

## First run

The first time the controls are shown, `Free camera` is switched on and the third-person camera speed is set to 2.0 *if the
player never chose a value*, because the vanilla camera is slow for a swipe and touch-look does nothing outside first person
without it. Both can be changed in the Touch Controls window or the Enhancements menu. This happens once (`gTouch.DefaultsApplied`).

## Tests

```sh
sh tests/touch-controls/run_tests.sh
```

Needs only a C++ compiler (built with address and undefined-behaviour sanitizers). The tests are outside `app/` on purpose:
the game's CMake globs every `.cpp` under `soh/Enhancements`, and the test program has its own `main()`.
The build workflow runs them too, without blocking the APK.

## Console variables

`gTouch.Enabled`, `.AutoHide`, `.Hidden`, `.Opacity`, `.Scale`, `.LeftHanded`, `.FloatingStick`, `.ShowDpad`, `.Haptics`,
`.LookSensitivity`, `.InvertLookX`, `.InvertLookY`, `.GyroAiming`, `.GyroSensitivity`, `.DefaultsApplied`, and `.Layout.<Control>.X/.Y`
(only once the layout has been edited; `gTouch.Layout.Custom`).
