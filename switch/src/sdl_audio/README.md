# SDL audio backend for the Switch

`SDL_switchaudio.c` replaces the audio backend inside devkitPro's `libSDL2.a`: the game links its own
`SWITCHAUDIO_bootstrap` first, so the library's is never pulled in. SDL itself is not modified.

The files come from [snowboardkids2-recomp-switch](https://github.com/D35P4C1T0/snowboardkids2-recomp-switch),
which adapted them from devkitPro SDL commit
[`0738d3c`](https://github.com/devkitPro/SDL/blob/0738d3c9f6993875e2f3dd0e8cc0bb4ae4440b4e/src/audio/switch/SDL_switchaudio.c)
(Switch SDL 2.28 branch). SDL's zlib license notices are kept in each file.

What changes: two 1024-frame buffers stay queued in the audio renderer, and after queueing one the backend waits
for a free slot, not for that buffer to start playing. That removes the gaps the stock backend produced when a
frame took longer than usual.

`SDL_sysaudio.h` is SDL's private audio interface. It is only valid for **SDL 2.28.5**, which the sources check
at compile time. Review it against the real library before moving to another SDL version.
