# Status

Version 0.1.0. What has been checked, how, and what still needs a console.

## Checked on the PC

| What | How it was checked |
| --- | --- |
| The ROM | SHA-1 compared with the one Bomberman Hero: Recompiled supports |
| Game code | N64Recomp turns the ROM into 4,864 C functions; RSPRecomp writes the audio microcode |
| Game patches | the MIPS patches compile and link with clang and lld, and recompile into 553 functions |
| Dependency patches | each applies to its pinned commit, and `tools/build.py deps` reports every repository as patched |
| Switch build | the runtime, RT64, the launcher and the game compile with devkitA64 and link with the Vulkan driver into an NRO |
| NRO | name "Bomberman Hero", author "hazevauks", 256x256 icon |

None of this says the game runs: it has not been started on a console or an emulator yet.

## Needs a console

Each line is a test for real hardware. A run leaves `bmhero_log.txt` in `sd:/switch/BombermanHeroNX/SD_LOG/`,
which is what to bring back, whatever happened.

| Test | Passes when |
| --- | --- |
| Start | the launcher appears, with its buttons and the frame rate in a corner |
| ROM | with `bmhero.z64` in the folder **Start Game** is available at once; with another name, **Select ROM** finds and accepts it |
| Boot | the Hudson logo and the title screen appear, with sound |
| Menus and HUD | text, icons and transparent layers look right; A chooses, B goes back |
| On foot | a stage of Planet Bomber plays from start to finish: jumps, throws, set bombs, camera |
| Vehicle stages | Bomber Jet, Marine, Copter and Slider control and draw correctly |
| Bosses and cutscenes | one boss fight and one story scene, start to finish |
| Saves | a save made in one run loads in the next |
| Settings menu | **−** opens it over the game; a changed option is kept after a restart |
| Leaving | the HOME button, sleep and wake, and quitting from the menu all work |
| Frame rate | the `frames:` lines of the log, handheld and docked, at the standard clocks |
| Memory | no crash after 30 minutes; the `memory:` line of the log at startup |

## Known limits

- **Album launches are refused.** From the album the homebrew menu has too little memory; the port explains this
  and returns. Start the homebrew menu from a game.
- **Mods that change game code are off.** Texture packs and data mods load; code mods need a way of loading code
  that Horizon allows, which is not written yet.
- **Memory.** The runtime sets aside 512 MB for the N64's address space, as on PC. It fits easily, but it is more
  than the game uses and can be trimmed.
- **First start is slow.** Shaders are compiled the first time each one is needed and kept in `cache/`. Expect
  hitches in the first minutes of a fresh install.
- **Tuning aids are on.** The frame rate is drawn on screen and stage timings go to the log. Empty files named
  `no-fps` and `no-profile` in `config/` turn them off.

## Performance target

A steady 60 fps at the standard clocks: CPU 1020 MHz, GPU 307 MHz handheld, memory 1331 MHz. The game's own
logic runs at 30 fps; RT64 draws the frames in between. No measurements exist yet. The log is set up to produce
them: a `clocks:` line at startup, `frames:` every five seconds, and `profile:` lines that split a frame into its
stages (display list, texture uploads, framebuffer copies, GPU wait, presentation).
