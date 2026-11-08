# Clean Dialogue HUD

An SKSE plugin for Skyrim Special Edition / Anniversary Edition that hides the mouse cursor and
the compass when a conversation starts. The cursor comes back as soon as you move the mouse, and the compass comes
back when the conversation ends.

Supported runtimes: 1.5.x, 1.6.x and 1.7.x in one DLL via CommonLibSSE-NG and Address Library,
built against 1.7.104.

This code was written with LLM assistance.

Requirements: [SKSE64](https://skse.silverlock.org/) and
[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).

## Notable Files

* Config: `SKSE\Plugins\CleanDialogueHUD.ini`
* Log: `Documents\My Games\Skyrim Special Edition\SKSE\CleanDialogueHUD.log`

## INI options

* **`[General]`**
  * `EnableLogging` (`false`): write `CleanDialogueHUD.log` with the loaded settings and
    every hide and show. `false` writes no log file, so unknown option warnings are only seen
    with logging on.
* **`[Cursor]`**
  * `HideCursor` (`true`): hide the mouse cursor when a conversation starts.
  * `MoveThreshold` (`2`): how far the mouse must move in one step (in raw mouse units) to
    show the cursor again. Smaller moves are treated as sensor jitter.
  * `ShowCursorOnClick` (`true`): a mouse click or wheel scroll also shows the cursor. That
    click or scroll does nothing else, so a click can never pick a line you couldn't see the
    cursor on.
  * `BlockHiddenHover` (`true`): while the cursor is hidden, keep it from highlighting a
    dialogue line. Jitter below `MoveThreshold` is ignored, and the pointer is parked off
    screen, so the line `E` picks is the one you chose with the keyboard. After the cursor
    comes back, move the mouse to point at a line again.
* **`[Compass]`**
  * `HideCompass` (`true`): hide the compass when a conversation starts.
  * `ShowCompassWithCursor` (`false`): show the compass again when the mouse brings the cursor
    back. With `false` it stays hidden until the conversation ends.

If a menu that uses the cursor opens on top of the conversation (barter, gift, training, the
console and so on), the cursor is shown for that menu and hidden again when you return to the
conversation. With a gamepad enabled the game already hides the cursor, so the plugin leaves it
alone and only hides the compass. A compass that is already hidden, for example with
`bShowCompass=0`, stays hidden: the plugin only changes the compass's transparency and puts back
the value it found. Loading a save or starting a new game puts back anything that was still
hidden. Unknown option names are reported as warnings in the log.

The plugin doesn't depend on `dialoguemenu.swf`, `hudmenu.swf` or `cursormenu.swf`, but it
expects the vanilla compass at `HUDMovieBaseInstance.CompassShoutMeterHolder.Compass`. If a HUD
replacer moves it, the log says the compass clip was not found and the compass is left alone.

## License

Clean Dialogue HUD, Copyright (C) 2026 hankinsrim

This program is free software: you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License for more details (`LICENSE`, or
<https://www.gnu.org/licenses/gpl-3.0.html>).

Source: <https://github.com/hankinsrim/CleanDialogueHUD>

It statically links [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng), which
is GPL-3.0-or-later with exceptions (see `extern/CommonLibSSE-NG/EXCEPTIONS.md`).
