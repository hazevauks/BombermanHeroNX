# How the port is put together

Bomberman Hero: Recompiled is not an emulator. [N64Recomp](https://github.com/N64Recomp/N64Recomp) translates the
game's MIPS machine code into C once, on a PC, and that C is compiled like any other program. Around it, three
libraries stand in for the console:

| Library | Stands in for | On the Switch |
| --- | --- | --- |
| [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) | the N64's operating system: threads, timers, controllers, saves, audio and video interfaces | patched: memory without `mmap`, no desktop thread APIs, paths on the SD card, no code-patching mods |
| [RT64](https://github.com/rt64/rt64) | the N64's graphics chip, reading the game's display lists | patched: runs on Vulkan through Mesa's NVK |
| [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) | nothing on the N64: the launcher, the settings menus, controller mapping | patched: fixed 1280x720 surface, no file picker, controller-first menus |

```text
your ROM ──(N64Recomp, on the PC)──► RecompiledFuncs/*.c ─┐
game patches (patches/*.c, MIPS) ──► RecompiledPatches/   ├─► BombermanHeroNX.nro
src/ + switch/src/ + the three libraries + libvulkan.a ───┘

on the console:  game code ─► N64ModernRuntime ─► RT64 ─► plume ─► Vulkan (NVK) ─► GPU
                                    │
                                    └─► SDL2 (controllers, audio)
```

## What is specific to the Switch

Everything of ours lives in `switch/`; the game's own sources in `src/` only gain a few `__SWITCH__` blocks.

| Path | What it is |
| --- | --- |
| `switch/cmake/` | `SwitchPlatform.cmake` (libraries, driver, host tools) and `SwitchTarget.cmake` (platform sources, NRO) |
| `switch/src/nx_main.cpp` | startup: how the game was launched, the SD card folders, renderer settings, fatal errors |
| `switch/src/nx_log.cpp` | `SD_LOG/bmhero_log.txt`, also sent to the PC under nxlink |
| `switch/src/nx_crash.cpp` | `SD_LOG/bmhero_crash.txt` when the process faults |
| `switch/src/nx_profile.cpp` | frame rate and per-stage timings for the log |
| `switch/src/sdl_audio/` | SDL audio backend that keeps two buffers queued |
| `switch/src/egl_stubs.c` | EGL symbols SDL wants at link time and never calls |
| `switch/src/mman.c` | `mmap` for the heap; refuses executable memory, which code mods would need |
| `switch/compat/` | POSIX headers newlib lacks |
| `switch/patches/` | the changes to the dependencies, one patch per repository |

### Graphics

RT64 was written for Direct3D 12, Vulkan and Metal on desktop GPUs. On the Switch it uses its Vulkan back end
(the [plume](https://github.com/renderbag/plume) layer) on top of NVK, Mesa's driver, which is linked into the NRO
as a static library. There is no window system: plume makes its Vulkan surface straight from libnx's window
(`VK_NN_vi_surface`), and SDL never opens a window of its own.

The Tegra X1 is far from the GPUs RT64 targets, so the Switch patches change how it works where that matters:

- N64 textures are decoded on the CPU and uploaded once, instead of being decoded by a shader on every draw.
- Framebuffer contents the game reads back are copied through one packed transfer.
- The shader the game needs for each material is compiled in the background, with RT64's general purpose shader
  covering until it is ready, and compiled pipelines are kept in `cache/` between runs.

### Memory

The recompiled code addresses the N64's memory through one base pointer. On a PC that region is 4 GB of reserved
address space; on Horizon it is an ordinary allocation that covers the console's RAM plus the area the patches
and mods use. Mods that replace game code at run time are off for now, since Horizon does not let a program
rewrite its own code the way a PC does; texture packs and data mods are not affected. The game must be started as an application (from a game, with title takeover), which gives it the
console's full memory; from the album it gets a fraction and the port says so instead of starting.

### Where the Switch changes come from

The dependency patches are rebased from
[snowboardkids2-recomp-switch](https://github.com/D35P4C1T0/snowboardkids2-recomp-switch) (GPL-3.0), the first
port of this family of recompilations to run RT64 on the Switch, onto the versions Bomberman Hero: Recompiled
pins. Its driver was a different NVK build; this port links the Mesa Horizon SDK instead, and has its own platform
layer, logging and build.
