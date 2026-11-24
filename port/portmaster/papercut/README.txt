Paper Cut for the R36S and other RK3326 handhelds (PortMaster)
==============================================================

A native port of Paper Mario: The Thousand-Year Door, built from the Paper Cut
decompilation, for Rockchip RK3326 / Mali-G31 handhelds (R36S and similar).
Source and credits: https://github.com/jimmygzzmtz/paper-cut-r36s

NO GAME DATA IS INCLUDED. You need your own legal copy of the European
release (PAL, game ID G8MP01) as a disc image.

INSTALL
1. Copy "Paper Cut.sh" and the "papercut" folder into /roms/ports/.
2. Put your disc image at /roms/ports/papercut/game.iso
   (any .iso / .gcm in the papercut folder is found too).
3. Start "Paper Cut" from Ports. The first start builds a shader cache, so
   new areas stutter briefly the first time you see them.

CONTROLS
  Select          Paper Cut's settings page (L1/R1 switch tabs, D-pad left/right
                  moves between the tab list and its settings)
  Select + Start  quit

OPTIONS (top of papercut/PaperCut.sh)
  GAME_LANGUAGE   en (default), de, fr, es, it, or auto
  CPU_MODE        performance (default; keeps your OS clock limit, including an
                  overclock) or balanced (cooler, longer battery)
  GX_THREAD       1 (default): graphics translation on its own CPU core
  TIMING_LOG, SAMPLE_PROFILE   performance logging for tuning (off)

Saves and settings live in papercut/saves/ (memory card: saves/EUR/Card A/).
The last run's log is papercut/log.txt.
