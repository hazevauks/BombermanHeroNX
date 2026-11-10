# Building

The build has two halves. Tools that run on your PC turn the ROM into C code and compile the shaders; then
devkitA64 cross-compiles everything into the NRO. `tools/build.py` runs both.

## What you need

| | For | Notes |
| --- | --- | --- |
| Your **Bomberman Hero** ROM | the game's code and data | North American version, `.z64`, SHA-1 `a36364b7e59351f7551ab351cb3b41ebc4be285b` |
| [devkitPro](https://devkitpro.org/wiki/Getting_Started) | the Switch build | packages below |
| LLVM with **clang**, **ld.lld**, CMake and Ninja | the host tools and the game patches | clang must have the MIPS back end (`clang --print-targets`); the official LLVM and MSYS2's `clang64` do |
| Python 3.8 or newer | `tools/build.py` | no extra modules |
| The Mesa Horizon SDK | the Vulkan driver | see below |

devkitPro packages:

```bash
dkp-pacman -S switch-dev switch-cmake switch-pkg-config switch-sdl2 switch-freetype switch-libexpat switch-libzstd switch-zlib
```

On Windows the script finds devkitPro in `C:\devkitPro` or `%USERPROFILE%\Downloads\devkitPro`, and LLVM in
`C:\msys64\clang64\bin` or `C:\Program Files\LLVM\bin`. Set `BMHERO_DEVKITPRO` or `BMHERO_LLVM` to use other
folders. On Linux and macOS, `DEVKITPRO` must be set and the LLVM tools must be on `PATH`.

### The Vulkan driver

The Switch has no Vulkan driver that homebrew can use, so the NRO carries its own: **NVK**, Mesa's open source
driver for NVIDIA GPUs, built for Horizon by [danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch).
Get its `mesa-<version>-switch-unified-horizon-sdk.zip` package (this port is built with 26.2.4) and unpack what
the build needs with:

```bash
python tools/build.py sdk path/to/mesa-26.2.4-switch-unified-horizon-sdk.zip
```

The headers and `libvulkan.a` land in `third_party/nvk-sdk/`, which git ignores. The step also rebuilds the
library's symbol index with `ranlib`: as shipped, the index leaves out the Rust runtime the driver's shader
compiler needs, and the link fails with hundreds of undefined `core::` and `alloc::` references.

## Building

```bash
git clone --recurse-submodules <this repository>
cd BombermanHeroNX
python tools/build.py all "path/to/Bomberman Hero (USA).z64"
```

`all` runs these steps, which can also be run one by one:

| Step | What it does |
| --- | --- |
| `deps` | applies `switch/patches/*.patch` to the dependencies in `lib/` |
| `host` | builds N64Recomp, RSPRecomp and file_to_c for your PC, into `build/host/` |
| `recomp <rom>` | checks the ROM, then writes the game as C code (`RecompiledFuncs/`, `rsp/`) and compiles and recompiles the game patches (`RecompiledPatches/`) |
| `nro` | configures and builds `build/switch/BombermanHeroNX.nro` |
| `sd` | fills `SD_CARD/`; with `--rom <rom>` your ROM goes in too |

The first Switch build compiles RT64, the launcher and about a hundred large generated files: expect it to take
from twenty minutes to more than an hour, depending on the PC. Later builds only compile what changed.

Everything made from the ROM (`bmhero.z64`, `RecompiledFuncs/`, `RecompiledPatches/`, `rsp/`,
`patches/patches.bin`) stays on your machine: git ignores it.

## Working on the dependencies

RT64, N64ModernRuntime and RecompFrontend are submodules pinned to the commits Bomberman Hero: Recompiled uses.
The Switch changes are kept as one patch per repository in `switch/patches/`. After editing files under `lib/`:

```bash
python tools/build.py deps --refresh
```

rewrites the patches from what is in `lib/`. Commit the patch files; the submodules themselves stay at their
pinned commits.

## Notes for Windows

devkitPro's CMake toolchain only runs inside devkitPro's MSYS2, so `nro` starts its `bash` for you. Two CMake
features are turned off there, precompiled headers and C++ module scanning: both write MSYS2 paths into files the
compiler reads, where they cannot be translated.

Git for Windows converts line endings by default, which breaks patches. `deps` turns the conversion off in each
dependency before applying its patch.
