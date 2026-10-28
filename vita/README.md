# Wesnoth for the PS Vita

A port of Battle for Wesnoth 1.18 to the PS Vita (VitaSDK, SDL2). Work in progress.

Status: boots to the title screen, menus and campaigns work with the controls, and a full
multiplayer game plays (AI vs AI tested), all in Vita3K. Text renders through the real Pango/Cairo
stack. Heap use in a game is about 160 MB of the 256 MB reserved. **Not yet tested on hardware.**

Known gaps: English only (translations are not built); multiplayer hosting is off (no local
server); in-game screenshots and the "touch" path of the controls are untested in the emulator.

## Layout

| Path | What |
|---|---|
| `deps/build-deps.sh` | Cross-builds what VitaSDK does not package: GLib, Pango, Cairo, fontconfig, FriBidi, libffi, PCRE2, libiconv, and a patched SDL2 |
| `deps/patches/<lib>/`, `deps/overlay/<lib>/` | The port's changes to those libraries (patches; added files) |
| `deps/make-patch.sh` | Regenerates a library's patch from edits in `deps/build/<lib>` |
| `configure.sh` | Configures the Wesnoth build in `vita/build` |
| `wesnoth-vita.cmake` | Vita link setup and `.vpk` packaging, included from `src/CMakeLists.txt` |
| `compat/` | Vita-only sources: libc fixes, ICU stubs, start-up settings, controls, exception tracing |
| `install-data.sh` | Puts the game data at `ux0:data/wesnoth` (Vita3K, or a copy for a real Vita) |
| `run.sh`, `screenshot.sh` | Build and run in Vita3K; capture the emulator window |
| `fonts.conf` | fontconfig setup, installed as `ux0:data/wesnoth/fonts.conf` |
| `sce_sys/` | LiveArea images |

## Building

```sh
# Packages from vdpm, once:
vdpm install libwebp sdl2_image curl pixman harfbuzz expat
vita/deps/build-deps.sh          # ~10 minutes, installs into $VITASDK
vita/configure.sh
cmake --build vita/build -j$(nproc)   # -> vita/build/src/wesnoth.vpk
```

`VITA_EXTRA_FLAGS` passed to `configure.sh` adds compiler flags, e.g. the debug switches
`-DVITA_DEBUG_SCREENSHOTS` (heap use logged every 5 s) and `-DVITA_DEBUG_INPUT`.

## Installing

1. Install `wesnoth.vpk` with VitaShell.
2. `vita/install-data.sh copy <dir>` and copy `<dir>` to the Vita as `ux0:data/wesnoth/` (about
   600 MB: `data`, `fonts`, `images`, `sounds`, `fonts.conf`).

AI-vs-AI soak tests in Vita3K: `vita/ai-test.sh "<scenario id> <sides> <turns> <minutes>" ...` plays
each game and prints a summary (how it ended, side turns, fps, slow frames, heap, AI time,
exceptions); logs in `vita/build/ai-test/`. Wesnoth 1.18 ignored `--exit-at-end`; fixed in
`game_launcher.cpp`.

Scripted input for testing in Vita3K: `ux0:data/wesnoth/vita-input-test.txt` (format in
`compat/vita-input.cpp`; button taps, stick, and keys such as `key ctrl o` for the load dialog).
Launch and loading steps are logged as `[vita] at <ms>: <step>`.

Extra command-line arguments can be put in `ux0:data/wesnoth/vita-args.txt`, one per line
(e.g. `--log-info=all`, or `--multiplayer` / `--controller=1:ai` / `--controller=2:ai` for an
AI-vs-AI game).

## Controls

| | |
|---|---|
| Touchscreen, on the map | drag to pan, tap to click |
| Touchscreen, elsewhere | mouse (finger down = left button held) |
| Left stick | pointer |
| Cross / Circle | left / right click |
| D-pad, right stick | arrow keys (scroll the map, move in lists) |
| L / R | zoom out / in |
| Square | next unit |
| Triangle | Enter |
| Start | Escape (menu, cancel) |
| Select | end turn |

## What the port changes, and why

Libraries (`deps/`):

- **GLib**: GIO replaced by `gio-lite` (only `GListModel`, all Pango uses); POSIX stubs for
  process/signal/user-database calls newlib lacks; short-enum and hostname fixes; threads created
  without inherited scheduling (an inherited priority is invalid on the Vita).
- **fontconfig**: no `random()`/`initstate`, no symlinks; cache locking falls back to a directory
  when hard links fail with ENOSYS, as on the Vita - before, the font cache was never written and
  every start read all the fonts again (9 s).
- **SDL2**: clip rectangles through GXM's region clip instead of the stencil buffer. With the
  stencil version, everything drawn under a clip into a render-target texture vanished, which is
  all of Wesnoth's GUI. The region clip only works in whole 32x32 tiles on real hardware (Vita3K
  clips to the pixel), and Wesnoth redraws just the changed parts of a persistent screen texture,
  so spills left black boxes and stale text (the in-game sidebar). Rectangles - every copy, fill
  and straight line, drawn as two triangles - are therefore cut to the clip on the CPU before
  drawing (`clip_quads`); the tile-aligned region clip remains as a backstop for other shapes.
- The vdpm packages' CMake files name `/usr/local/vitasdk`; `build-deps.sh fixups` repoints them.

Wesnoth (`src/`, all under `#ifdef __vita__`):

- `wesnoth.cpp`: data at `/data/wesnoth` with the working directory on `ux0:` (Boost treats
  `ux0:...` as relative; newlib maps a leading `/` to the current device); `FONTCONFIG_FILE`;
  Boost.Locale's `std` backend (VitaSDK's ICU has no data); extra arguments from `vita-args.txt`.
- `sdl/window.cpp`: SDL's native GXM renderer (the default OpenGL ES one needs `libshacccg.suprx`).
- `seed_rng.cpp`, `random.cpp`: the kernel RNG instead of `/dev/urandom`.
- `filesystem.cpp`, `game_launcher.cpp`: no Boost.Process (no processes to launch).
- `events.cpp`, `video.cpp`: the controls layer and its pointer.
- `gettext.cpp`: case-insensitive sorting by ASCII lowercase, as on macOS (the `std` backend has no
  collator; the generic path threw and caught on every comparison).
- `gui/widgets/text_box*.cpp`, `controller_base.cpp`, `wesnoth.cpp`: text input - which on the
  Vita is the system keyboard - starts only when a text box is tapped and stops when it loses
  focus, instead of being on all the time.
- `preferences/general.cpp`: "Animate water" off by default. Every frame of the water and coast
  waves is a separate image masked per hex shape (the waves are 180x216, 13 frames); building them
  stalled scrolling, and keeping them all ran the 256 MB heap out.
- `image_modifications.cpp` (all platforms, same results): the formulas the engine writes itself -
  units standing in water (`~ADJUST_ALPHA(if(y>N,...))`) and the white flash on hits
  (`~CHAN(255,255,255,alpha)`) - are worked out directly instead of through the formula
  interpreter, which took 100-200 ms an image on the Vita, for every animation frame.
- `display.cpp`: units far off screen are not asked which hexes their animation covers each frame.
- `game_display.cpp`: clock, battery and turn-timer reports refreshed once a second, and the rest of
  the sidebar at most ten times a second (in fights it was rebuilt every frame).
- `preferences/general.cpp`: the minimap in terrain colours by default (drawn from terrain images,
  rebuilding it on each unit selection and fight took a large part of a second on big maps).
- `units/animation.cpp`, `scripting/game_lua_kernel.cpp`: no 10 ms sleep between frames while an
  animation or a scripted delay plays. Drawing waits for the display's refresh already; the sleep
  on top made every frame miss the next refresh, so moves and fights ran at 30 fps.
- `gui/gui.cpp`: the GUI's WML read through the config cache, without the schema check: preprocessing
  and checking it on every start was more than half of the start-up time (2.6 of 4.3 s in Vita3K).
- `filesystem_common.cpp`: the data checksum the caches are checked against is `vita-index.txt`'s size
  and time, not a walk of the data (which, from the working directory `ux0:`, walked all of
  `ux0:data` - 6 s). Data changed without a new index will not refresh the caches.
- `filesystem.cpp`: game data directories listed from the index (`vita-index.txt`), not the card.
- `terrain/builder.cpp`: a terrain rule's images are loaded when it first applies, not when it is read;
  most of the 17 000 rules (with rotations) are for terrains a map does not have. Loading a game
  spent most of a minute on them. Same terrain either way (checked on two maps).
- `filesystem.cpp`: a file being replaced is removed first; Vita3K does not truncate on open, which
  left the save index unreadable.
- Memory (`gui/dialogs/title_screen.cpp`, `draw_manager.cpp`, `sound.cpp`): the image and text caches
  are emptied on returning to the title screen; over 200 MB of heap, cached image pixels are let go
  (textures kept); at most 64 sounds kept decoded. A long game held ~180 of the 256 MB otherwise.
- `gui/core/canvas.cpp`, `gui/widgets/styled_widget.cpp`: widgets made from the same definition share
  its parsed drawing shapes instead of each parsing their own.
- Add-ons manager (`addon/client.cpp`, `data/gui/widget/addon_list.cfg` under the `PSVITA` define,
  set in `config_cache.cpp`): the list is asked for without icons, and its rows are the narrow-screen
  ones without per-row buttons (the details pane has them). 800+ add-ons with both took more memory
  than the Vita has, and with icons the layout never finished. `ux0:data/wesnoth/vita-addons-list.gz`
  (a saved server reply) stands in for the server under Vita3K, whose networking crashes it.
- `gui/widgets/scroll_label.cpp`, `gui/dialogs/game_load.cpp`: a scroll label given new text lays it
  out in its own space; before, text wider than it made the whole window lay itself out again - 4 to
  6 s in the add-ons manager each time another add-on was picked.
- `tooltips.cpp`: a tooltip's text is rendered when it is first shown, not when it is made. The
  sidebar remakes a tooltip for every piece of every report each time another unit is hovered.
- `sound.cpp`: sound effects from the pack (`compat/vita-pack.cpp`), stored there already decoded
  as WAV; Vorbis decoding took ~70 ms a sound, mostly in the middle of fights.
- `src/CMakeLists.txt`: no build-machine SDL tests; includes `vita/wesnoth-vita.cmake`.

Relocations: the Vita does not always load the game at its link address (Vita3K does), so the ELF
must keep its relocations (`-Wl,-q`, set in `configure.sh` because Wesnoth's CMakeLists drops the
toolchain's), and no library may contain position-independent code - `vita-elf-create` rejects its
GOT relocations. Meson builds static libraries with `-fPIC` by default; `vita-cross.ini` turns that
off, and GLib's gnulib, which forces it, is patched.

Fonts (`compat/vita-fonts.c`): FreeType reads font files piecemeal, a table or glyph outline at a
time, and on the memory card each read is slow - new text sizes and styles took up to a second
to draw. `FT_New_Face` is wrapped at link time to read each font file whole, once, and open it
from memory.

Images (`make-pack.py`): all images in one file, `vita-images.pak`, read with one call each
instead of opening a file each (~20 ms). Images far larger than the screen are stored shrunk
(`REDUCE`; the 4096x2160 title/loading background took seconds to decode), appended after the
originals so an already-uploaded pack only needs its new end sent: `curl -C - -T`.

Link-time (`wesnoth-vita.cmake`): all of libpthread is linked, otherwise libstdc++ decides there
are no threads and `std::call_once` fails; ICU libraries last; unused ICU break-iteration symbols
stubbed (`compat/icu-stubs.c`).

## Debugging

Every C++ exception is printed with a backtrace (`compat/vita-debug.cpp`):

```sh
arm-vita-eabi-addr2line -f -C -e vita/build/src/wesnoth <addresses>
```

In Vita3K the game's stderr appears in the emulator log as `TTY` lines (`vita/run.sh` keeps it in
`vita/build/vita3k.log`). In-game screenshots come out black under Vita3K; `vita/screenshot.sh`
captures the emulator window instead (Hyprland + grim).
