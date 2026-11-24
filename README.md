# Paper Cut for the R36S (RK3326 / PortMaster)

A native port of **Paper Mario: The Thousand-Year Door** for **Rockchip RK3326 / Mali-G31** handhelds — the R36S
and similar devices — through [PortMaster](https://portmaster.games/). It runs the
[Paper Cut](https://github.com/LLCOOLT/paper-cut) decompilation on a native OpenGL ES 3.0 build of Aurora, and is a
fork of [jbcgames' RK3326 port](https://github.com/jbcgames/paper-cut) with fixes and a round of performance work.

> **No game data is included.** Neither this repository nor its releases contain the game, a disc image, or any file
> taken from the disc. You need your own legal copy of the **European release (PAL, game ID `G8MP01`)**.

**Tested on:** a Soysauce R36S running dArkOSen (Mali-G31 blob driver), European disc.

---

## Install

1. Download `papercut-<version>-portmaster.zip` from [Releases](../../releases) and check it against `SHA256SUMS.txt`.
2. Unzip it into your ports folder (`/roms/ports/`), so you have `/roms/ports/Paper Cut.sh` and `/roms/ports/papercut/`.
3. Put your disc image at `/roms/ports/papercut/game.iso` (any `.iso` / `.gcm` in that folder is found too).
4. Start **Paper Cut** from Ports.

Coming from jbcgames' release? Replace `ttyd-eu` and `PaperCut.sh` in `papercut/`; your saves in `papercut/saves/`
carry over.

### Controls

| Button | Action |
| --- | --- |
| Select | Paper Cut's settings page (L1/R1 switch tabs; D-pad left/right moves between the tab list and its settings) |
| Select + Start | Quit |

### Options (top of `papercut/PaperCut.sh`)

| Variable | Default | What it does |
| --- | --- | --- |
| `GAME_LANGUAGE` | `en` | `en`, `de`, `fr`, `es`, `it`, or `auto` (the settings file / system) |
| `CPU_MODE` | `performance` | Holds the CPU at your OS's clock limit (an overclock included). `balanced`: schedutil, cooler, longer battery |
| `GX_THREAD` | `1` | GameCube graphics translation on its own CPU core. `0`: everything on the game's thread, as before |
| `TIMING_LOG` | `0` | Per-frame timing in `papercut/log.txt` (frame phases, GL call costs, GX thread, swap) |
| `SAMPLE_PROFILE` | `0` | A sampling CPU profiler: `papercut/profile.bin` + `profile.maps` |

Saves and settings are in `papercut/saves/` (memory card: `saves/EUR/Card A/`); the last run's log is
`papercut/log.txt`. The game renders 30 frames a second with its logic at 60 (battery saver "Always", the default).

---

## Performance

Measured on the R36S at its OS clock limit, 640×480, 30 FPS mode:

| Scene | Starting point (jbcgames' port) | This fork |
| --- | --- | --- |
| Outside areas, sewers | mid 20s, big dips | 30, occasional dips |
| Rogueport, walking around (the heaviest town) | ~20 FPS, multi-second freezes | mid-to-high 20s |

Expect a short stutter the first time an area, effect or texture appears (shaders and textures are built and uploaded
once, then cached) and during room changes.

## What's new in this fork

**Fixes**
- **Readable overlays and settings page.** Every Dear ImGui text (the FPS counter, Select's settings page) drew its
  glyphs as black boxes: the GLES backend uploaded the font atlas before the game added its fonts and never again.
- **Your CPU overclock is kept.** The launch script forced the CPU's maximum clock down to 1.2 GHz on every launch
  (and never put it back); it now keeps the OS's limit and restores the governor on exit.
- **No 2-second freezes from quick resume.** Its once-a-minute save is written on a background thread (one write to
  the SD card stalled the game for 2.4 s).
- **Settings page by controller.** Focus starts on the current tab, D-pad left/right moves between the tab list and
  its settings, a highlighted tab shows its settings.
- **No loading splash**, English by default, ~30 MB less RAM for the menu fonts on small screens.

**Performance** (each measured on the device with the built-in profilers)
- **GX thread.** Translating GameCube GX commands into OpenGL was about 45% of the game thread's work in Rogueport;
  it now runs on its own core alongside the game, following the GameCube's own model (the GPU read the command FIFO
  while the CPU ran on).
- **Streamed render passes.** The frame is handed to the render thread in pieces while it is still being recorded,
  instead of all at once at the end, so the two threads work side by side instead of taking turns.
- **Cheaper Mali draws.** Indexed draws pass their index range, so the Mali driver no longer reads every draw's
  indices on the CPU (~55 → ~37 µs per draw).
- **Game-side hot spots.** Collision checks without a square root per object, cached map-object and material lookups
  by name (thousands of string compares per frame in Rogueport), cached settings and vertex-array lookups.
- **Steadier pacing.** Frames are presented on the 60 Hz tick with precise waits; first-time shaders no longer freeze
  the game while they compile (`no_pop_in=0`).

The Aurora changes live in **[aurora-r36s](https://github.com/jimmygzzmtz/aurora-r36s)** (the `extern/aurora`
submodule). They are in Aurora's GX and GL layers rather than in game code, so other Aurora-based GameCube ports on
RK3326/Mali-G31 handhelds can use them.

### Known limits

- Rogueport and other crowded scenes don't hold a locked 30 FPS: the game thread and the Mali driver's per-draw cost
  are both near the frame budget there.
- Paper Cut's newer upstream features (0.5.x, including Paper Mario 64 support) aren't merged; this fork is based on
  Paper Cut 0.4.1 as jbcgames' port was.
- Only the European (`G8MP01`) build is packaged.

---

## Building

Cross-compiled from Linux or WSL (x86_64) with clang 21 against an Ubuntu 24.04 arm64 sysroot (glibc 2.39, which
dArkOS-class firmware has), linking the same SDL3-over-SDL2 shim the package ships.

```bash
sudo apt install clang-21 lld-21 llvm-21 cmake ninja-build git python3 binutils-aarch64-linux-gnu
git clone --recursive https://github.com/jimmygzzmtz/paper-cut-r36s.git && cd paper-cut-r36s

# the arm64 sysroot (no root needed)
python3 tools/rk3326/make_sysroot.py ~/rk3326-build/sysroot

# the SDL3-over-SDL2 shim: libSDL3.so.0 from a release zip's papercut/libs.aarch64/, plus SDL 3.4.0's headers
mkdir -p ~/rk3326-build/sdl3-shim && cp <release>/papercut/libs.aarch64/libSDL3.so.0 ~/rk3326-build/sdl3-shim/
ln -s libSDL3.so.0 ~/rk3326-build/sdl3-shim/libSDL3.so
curl -L https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz | tar xz
cp -r SDL3-3.4.0/include ~/rk3326-build/sdl3-shim/

export RK3326_SYSROOT=~/rk3326-build/sysroot
cmake -S . -B build-aarch64 -G Ninja --toolchain=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release -DTTYD_VERSION=G8MP01 -DTTYD_BACKEND=aurora -DTTYD_AURORA_GLES=ON \
  -DTTYD_NO_DISC_ASSETS=ON -DTTYD_PORT_VERSION=0.5.0-r36s.1 \
  -DAURORA_SDL3_SHIM_DIR=$HOME/rk3326-build/sdl3-shim
ninja -C build-aarch64 ttyd-eu
aarch64-linux-gnu-strip -o ttyd-eu build-aarch64/ttyd-eu
```

The stripped `ttyd-eu` replaces `papercut/ttyd-eu` in the package; the package's other files are in
`port/portmaster/`. jbcgames' original notes on the RK3326 port (in Spanish) are in
[docs/RK3326_PORT.md](docs/RK3326_PORT.md).

---

## Credits

- **[LLCOOLT/paper-cut](https://github.com/LLCOOLT/paper-cut)** — Paper Cut: the decompilation of The Thousand-Year
  Door and its native PC port, which everything here is built on.
- **[jbcgames](https://github.com/jbcgames)** — the RK3326 / PortMaster port this is forked from
  ([jbcgames/paper-cut](https://github.com/jbcgames/paper-cut)) and Aurora's OpenGL ES 3.0 backend for the Mali-G31
  ([jbcgames/aurora](https://github.com/jbcgames/aurora), `rk3326-gles`), its fixes and packaging.
- **[encounter/aurora](https://github.com/encounter/aurora)** and its contributors — Aurora, the GameCube/Wii
  platform layer.
- **bmdhacks** — the SDL3-over-SDL2 shim the package runs on.
- **[PortMaster](https://portmaster.games/)** — the ports platform and launch tooling.
- **jimmygzzmtz** — this fork: testing on the R36S and the work described above, done with Claude (Anthropic).

Paper Mario is a trademark of Nintendo; this project is not affiliated with or endorsed by Nintendo. Licenses:
[LICENSE](LICENSE) and third-party notices in [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
