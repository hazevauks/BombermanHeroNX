# SD_LOG

Logs brought back from the console, to look at a crash, a glitch or a slow scene. On the SD card they are in
`sd:/switch/BombermanHeroNX/SD_LOG/`:

| File | What it is |
| --- | --- |
| `bmhero_log.txt` | everything the game printed in its last run, with timestamps |
| `bmhero_log_previous.txt` | the same, for the run before |
| `bmhero_crash.txt` | a crash: the exception, the registers and the call stack |
| `bmhero_fatal.txt` | the message of a fatal error |

Copy them here after a test. Only this README is kept in the repository.

## Reading a crash

The addresses in `bmhero_crash.txt` are offsets into the program. With the ELF of the same build:

```bash
aarch64-none-elf-addr2line -f -C -e build/switch/BombermanHeroNX.elf 0x<offset>
```

## Reading the timings

While the port is being tuned the log gets a `frames:` line every five seconds (frame rate, the slowest frame,
how many frames were late) and `profile:` lines with the time each stage of a frame took, in microseconds.

## Switches for testing

An empty file with one of these names in `sd:/switch/BombermanHeroNX/config/` changes how the next run behaves.
The log says which ones it found.

| File | Effect |
| --- | --- |
| `no-fps` | hides the frame rate on screen |
| `no-profile` | leaves the `profile:` and `traffic:` lines out of the log |
| `rgba-copyback` | framebuffer copies the game reads back use plain RGBA instead of the packed format |
| `staged-transfers` | those copies go through a staging step instead of one transfer |
| `nvk-native-completion` | the Vulkan driver waits for the GPU with native fences |
| `nvk-cached-memory` | the Vulkan driver uses CPU-cached memory for its command and stream buffers |

The last four trade speed for a more conservative path. They are there to narrow down a graphics problem: if
a glitch goes away with one of them, say which in the report.
