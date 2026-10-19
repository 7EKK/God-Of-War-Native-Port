GOD OF WAR - PC
===============

FIRST RUN / IMPORT YOUR OWN GAME
--------------------------------
This release requires a compiled native game runner in bin\GodOfWar.exe.
A development-tools-only artifact is NOT a playable game release.

1. Place your own God of War (USA, SCUS-97399) ISO beside "God of War.exe",
   or keep it elsewhere and select it with the file picker.
2. Start "God of War.exe". It will offer the adjacent ISO or ask whether you
   want to select an ISO or a previously extracted disc folder.
3. ISO imports unpack the dual-layer disc to data\ and create .disc_layout
   and .disc_meta, with the correct original sector numbers. No internet,
   Python, Visual Studio, or separate extraction tool is required.
4. For already-extracted files, select the folder containing SCUS_973.99,
   PART1.PAK, PART2.PAK, GODOFWAR.TOC, .disc_layout, and .disc_meta.
   A folder of plain extracted files without both hidden dot files is not
   sufficient for this game's sector-based reads: choose an ISO instead.
5. After first import, start "God of War.exe" normally. The original ISO
   is not read during gameplay; you can archive it after verifying setup.

On Windows, .disc_layout and .disc_meta may not be visible with default
Explorer settings. Import checks source files before changing data\, and
does not modify saves or settings. If data\ is incomplete, move it out of
the way before trying again. Failed imports clean up their staging files.

Start "God of War.exe". The settings window sets the resolution, fullscreen and languages; press Play.
"God of War.exe --play" starts directly with the last settings.

Languages
---------
Text and voices can be English, French, German, Italian or Spanish (from the European
release of the game, when it was available while this folder was made; otherwise English
only). Text covers the in-game texts (hints, items, books, treasures); menus and the
intro movie stay English. The game has no subtitles for spoken lines.

In game
-------
F11            switch between window and fullscreen
Alt+F4         quit (save first: the game saves at its save points)

Controls (keyboard, used when no gamepad is connected)
------------------------------------------------------
Arrows / WASD  directional buttons
X / Space      Cross   (jump, confirm)
C / Esc        Circle  (grab, back)
Z              Square  (light attack)
V              Triangle (heavy attack)
Q / E          L1 / R1
Left/Right Shift  L2 / R2
Enter          Start   (pause menu)
Tab            Select

Controls (gamepad)
------------------
An Xbox-style gamepad (connected before starting): the buttons map like a PlayStation 2 controller
(A = Cross, B = Circle, X = Square, Y = Triangle, LB/RB = L1/R1, LT/RT = L2/R2,
Start/Back = Start/Select, both sticks, stick clicks = L3/R3).

Saves
-----
Your progress is in the "saves" folder next to this file:
  saves\GodOfWar.sav      all your saved games
  saves\GodOfWar.sav.bak  the same file as it was before the last start
To back up your progress, copy the "saves" folder. To go back to the state
before the last session, replace GodOfWar.sav with GodOfWar.sav.bak.

Folders
-------
bin\           the game program
data\          the game data
saves\         your saves
settings.ini   the settings from the start window
