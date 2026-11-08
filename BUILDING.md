# Building Clean Dialogue HUD

Requirements: Visual Studio 2022 or newer with *Desktop development with C++* (includes CMake,
Ninja and vcpkg), and Git.

## Building the plugin

```
git clone --recursive https://github.com/hankinsrim/CleanDialogueHUD.git
cd CleanDialogueHUD
powershell -ExecutionPolicy Bypass -File .\build.ps1
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Zip
```

* `build.ps1` enters the VS developer environment, uses `%VCPKG_ROOT%` if set (otherwise VS's
  bundled vcpkg), configures with the `release` preset and builds. The first build compiles
  the vcpkg dependencies and CommonLibSSE-NG, which takes a while.
* `-Zip` also writes `build\CleanDialogueHUD-<major.minor>.zip`, a minimal FOMOD whose
  `fomod\info.xml` carries the name and version for mod managers that read it.
* `-Preset debug` builds the debug configuration.
* `-Lint` compiles the plugin with MSVC Code Analysis (`/analyze`) and then runs VS's
  `clang-tidy.exe` over `src\` with the checks in `.clang-tidy`. The next build without `-Lint`
  switches the analysis off again.
* Manual equivalent from a *Developer PowerShell for VS*: `cmake --preset release` then
  `cmake --build --preset release`.

The mod-manager-ready output is staged in `dist\`:

```
dist\
    SKSE\
        Plugins\
            CleanDialogueHUD.dll
            CleanDialogueHUD.ini
            CleanDialogueHUD.pdb
```

Sources are formatted with `.clang-format` (VS ships `clang-format.exe` under
`VC\Tools\Llvm\x64\bin`).

## How it works

* A `MenuOpenCloseEvent` sink watches the `Dialogue Menu`. All Scaleform calls are queued with
  `SKSE::GetTaskInterface()->AddUITask`.
* Cursor: the `Cursor Menu` movie is hidden with `GFxMovieView::SetVisible(false)`, never with
  Win32 `ShowCursor`. If the `Cursor Menu` opens after the `Dialogue Menu`, or is reopened, the
  new movie is hidden too. Any other menu with `kUsesCursor` or `kUpdateUsesCursor` that opens
  during the conversation shows the cursor. When such a menu closes, a UI task checks the menu
  stack, and if the `Dialogue Menu` is the only menu left that uses the cursor, it is hidden
  again.
* Input: `MenuControls::ProcessEvent` (slot 1 of its `BSTEventSink<InputEvent*>` vtable) is
  hooked. While the cursor is hidden, a `MouseMoveEvent` with a step of at least
  `MoveThreshold` shows it, smaller steps are unlinked from the event chain before the menus
  see them, and the click or wheel event that shows the cursor is unlinked too (a click stays
  swallowed until its release). The chain is linked back before the next sink sees it.
* Hover: `dialoguemenu.swf` selects a line from each entry's `onRollOver`. When the cursor is
  hidden, `NotifyMouseState` parks the `Dialogue Menu` movie's pointer off screen, so a line
  scrolling under a still pointer can't take the selection.
* Compass: `hudmenu.swf` sets `_visible` on
  `HUDMovieBaseInstance.CompassShoutMeterHolder.Compass` every frame from `SetCompassAngle`
  (driven by `bShowCompass`), and `ShowElements` resets `_visible` on HUD mode changes. The
  plugin changes only that clip's alpha, saves the value it found, and puts it back only if
  the alpha is still 0. An alpha that is already 0 is left alone.
* `kPostLoadGame` and `kNewGame` show anything still hidden and clear the state.
