# Status

Version 0.1.0. What has been checked, how, and what still needs testing.

## Tested on a console

One session on 2026-11-10: a Switch on Horizon 23.0.1 with Atmosphère, handheld, at the standard clocks
(CPU 1020 MHz, GPU 307.2 MHz, memory 1331.2 MHz), starting from an empty shader cache. The player's report:
it started and ran perfectly. The log of that session (198 seconds) says:

| | |
| --- | --- |
| Screen | 1280x720, vertical sync, three swapchain images |
| Errors | none: no Vulkan error, GPU fault or lost device |
| Memory given to the process | 3,189 MB |
| First 85 seconds | 44 to 60 fps, with stalls of up to half a second while shaders were compiled |
| The 110 seconds after that | 59.4 to 60.0 fps in every 5-second window; slowest frame 34 ms |
| Game frames | 30 per second, as on the N64, with RT64 drawing the frame in between |
| Display list stage, per game frame | 12.7 ms on average, of the 33.3 ms available |
| Waiting for the GPU, per frame | 3.4 ms on average |

So the target, 60 fps at the standard clocks, is met in handheld once the shaders are in the cache.

## Checked on the PC

| What | How it was checked |
| --- | --- |
| The ROM | SHA-1 compared with the one Bomberman Hero: Recompiled supports |
| Game code | N64Recomp turns the ROM into 4,864 C functions; RSPRecomp writes the audio microcode |
| Game patches | the MIPS patches compile and link with clang and lld, and recompile into 553 functions |
| Dependency patches | each applies to a clean copy of its pinned commit |
| NRO | name "Bomberman Hero", author "hazevauks", 256x256 icon; only Mesa 26.2.4 inside |

## Not checked yet

Each line is a test for real hardware. A run leaves `bmhero_log.txt` in `sd:/switch/BombermanHeroNX/SD_LOG/`,
which is what to bring back, whatever happened.

| Test | Passes when |
| --- | --- |
| Docked | the game runs at 60 fps on a TV; the `frames:` lines of the log |
| The whole game | every world plays from start to finish, vehicle stages and bosses included |
| Cutscenes | the story scenes play with picture and sound in step |
| Saves | a save made in one run loads in the next |
| Settings menu | **−** opens it over the game; a changed option is kept after a restart |
| Leaving | the HOME button, sleep and wake, and quitting from the menu all work |
| Second start | with the shader cache filled, the first minute no longer stutters |
| Long session | no crash or slowdown after 30 minutes |

## Known limits

- **Album launches are refused.** From the album the homebrew menu has too little memory; the port explains this
  and returns. Start the homebrew menu from a game.
- **The first start stutters.** Shaders are compiled the first time each one is needed and kept in `cache/`.
- **Mods that change game code are off.** Texture packs and data mods load; code mods need a way of loading code
  that Horizon allows, which is not written yet.
- **The start of the log can be scrambled.** The port and its libraries write to the log file through separate
  handles, and their first lines overwrite one another. The timings further down are not affected.
- **Memory.** The runtime sets aside 512 MB for the N64's address space, as on PC. It fits easily, but it is more
  than the game uses and can be trimmed.
- **Tuning aids are on.** The frame rate is drawn on screen and stage timings go to the log. Empty files named
  `no-fps` and `no-profile` in `config/` turn them off.

## Reading the log

A `clocks:` line at startup, a `frames:` line every five seconds (frame rate, slowest frame, late frames) and
`profile:` lines that split a frame into its stages: display list, texture uploads, framebuffer copies, GPU wait,
presentation. [SD_LOG/README.md](../SD_LOG/README.md) lists the files and the switches for testing.
