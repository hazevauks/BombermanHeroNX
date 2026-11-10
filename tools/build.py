#!/usr/bin/env python3
"""Builds BombermanHeroNX, one step at a time or all at once.

    python tools/build.py all <rom>      every step below, in order
    python tools/build.py deps           apply switch/patches to the dependencies in lib/
    python tools/build.py host           build the tools that run on this PC
    python tools/build.py recomp <rom>   turn the ROM and the game patches into C code
    python tools/build.py nro            cross-compile the NRO with devkitA64
    python tools/build.py sd [--rom R]   lay out SD_CARD/, ready to copy to the card
    python tools/build.py release        zip what is in SD_CARD/ for a release, without the ROM

    python tools/build.py sdk <zip>      unpack the Vulkan driver SDK into third_party/nvk-sdk (once)
    python tools/build.py deps --refresh rewrite switch/patches from the changes made in lib/

What it needs is listed in docs/building.md. Nothing here downloads anything.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOST_DIR = ROOT / "build" / "host"
SWITCH_DIR = ROOT / "build" / "switch"
SDK_DIR = ROOT / "third_party" / "nvk-sdk"
PATCH_DIR = ROOT / "switch" / "patches"
SD_APP_DIR = ROOT / "SD_CARD" / "switch" / "BombermanHeroNX"
EXE = ".exe" if os.name == "nt" else ""

# The only ROM the recompilation is made for: Bomberman Hero (USA), big-endian (.z64).
ROM_SHA1 = "a36364b7e59351f7551ab351cb3b41ebc4be285b"
ROM_NAME = "bmhero.z64"

# Dependency folder, patch file. Outer folders first: a refresh leaves nested repositories out of their parent's patch.
DEPENDENCIES = [
    ("lib/N64ModernRuntime", "n64modernruntime.patch"),
    ("lib/RecompFrontend", "recompfrontend.patch"),
    ("lib/RecompFrontend/recompui/lib/RmlUi", "rmlui.patch"),
    ("lib/RecompFrontend/recompui/lib/lunasvg", "lunasvg.patch"),
    ("lib/rt64", "rt64.patch"),
    ("lib/rt64/src/contrib/implot", "implot.patch"),
    ("lib/rt64/src/contrib/plume", "plume.patch"),
    ("lib/rt64/src/contrib/plume/contrib/volk", "volk.patch"),
]

# Newer clang turns these old-C habits of the game patches into errors; the PC build's older clang only warns.
PATCH_WARNINGS = [
    "-Wno-error=incompatible-pointer-types",
    "-Wno-error=int-conversion",
    "-Wno-error=implicit-function-declaration",
    "-Wno-error=implicit-int",
]


class BuildError(Exception):
    pass


def step(title: str) -> None:
    print(f"\n== {title}", flush=True)


def run(command: list, cwd: Path = ROOT, env: dict | None = None, quiet: bool = False) -> None:
    command = [str(part) for part in command]
    if not quiet:
        print("  $ " + " ".join(command), flush=True)
    result = subprocess.run(command, cwd=cwd, env=env)
    if result.returncode != 0:
        raise BuildError(f"command failed ({result.returncode}): {' '.join(command)}")


def git(repository: Path, *arguments: str, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(repository), *arguments], capture_output=True, text=True, check=check)


# ---- dependencies


def dependency_state(repository: Path, patch: Path) -> str:
    """'clean', 'patched' or 'other', comparing the working tree with the pinned commit and the patch."""
    if git(repository, "apply", "--check", str(patch), check=False).returncode == 0:
        return "clean"
    if git(repository, "apply", "--reverse", "--check", str(patch), check=False).returncode == 0:
        return "patched"
    return "other"


def use_lf_line_endings(repository: Path) -> None:
    """The patches are made against the files as they are stored. Git for Windows may have checked them out with
    CRLF line endings, which no patch survives, so a clean repository is checked out again without conversion."""
    if git(repository, "config", "core.autocrlf", check=False).stdout.strip() == "false":
        return
    git(repository, "config", "core.autocrlf", "false")
    if git(repository, "status", "--porcelain", "--ignore-submodules=all").stdout.strip():
        return
    git(repository, "rm", "--cached", "-r", "-q", ".")
    git(repository, "reset", "--hard", "-q")


def command_deps(arguments: argparse.Namespace) -> None:
    step("Dependencies" + (" (refreshing the patches)" if arguments.refresh else ""))
    if not (ROOT / "lib" / "rt64" / "CMakeLists.txt").exists():
        raise BuildError("the submodules are missing: git submodule update --init --recursive")

    for folder, patch_name in DEPENDENCIES:
        repository, patch = ROOT / folder, PATCH_DIR / patch_name

        if arguments.refresh:
            # Everything that differs from the pinned commit, new files included, nested repositories left out.
            git(repository, "add", "-A")
            diff = subprocess.run(
                ["git", "-C", str(repository), "diff", "--cached", "HEAD", "--ignore-submodules=all", "--no-color"],
                capture_output=True, check=True).stdout
            git(repository, "reset", "-q")
            patch.parent.mkdir(parents=True, exist_ok=True)
            patch.write_bytes(diff)
            print(f"  {patch_name:28} {len(diff):>8} bytes from {folder}")
            continue

        use_lf_line_endings(repository)
        state = dependency_state(repository, patch)
        if state == "clean":
            git(repository, "apply", "--whitespace=nowarn", str(patch))
            print(f"  {folder}: patch applied")
        elif state == "patched":
            print(f"  {folder}: already patched")
        else:
            raise BuildError(
                f"{folder} matches neither its pinned commit nor {patch_name}.\n"
                "  If the changes there are yours, keep them with: python tools/build.py deps --refresh\n"
                f"  To drop them: git -C {folder} checkout -- . && git -C {folder} clean -fd")


# ---- host tools


def host_environment() -> dict:
    """PATH with an LLVM that has clang (with the MIPS back end), ld.lld, CMake and Ninja."""
    candidates = [os.environ.get("BMHERO_LLVM"), "C:/msys64/clang64/bin", "C:/Program Files/LLVM/bin"]
    environment = dict(os.environ)
    for candidate in candidates:
        if candidate and (Path(candidate) / f"clang{EXE}").exists():
            environment["PATH"] = str(Path(candidate)) + os.pathsep + environment["PATH"]
            break

    for tool in ("clang", "ld.lld", "cmake", "ninja"):
        if shutil.which(tool, path=environment["PATH"]) is None:
            raise BuildError(f"'{tool}' was not found. Put LLVM, CMake and Ninja on PATH, or set BMHERO_LLVM to "
                             "the folder with clang (docs/building.md).")

    clang = shutil.which("clang", path=environment["PATH"])
    targets = subprocess.run([clang, "--print-targets"], capture_output=True, text=True).stdout
    if "mips" not in targets:
        raise BuildError(f"{clang} cannot produce MIPS code, which the game patches are written in.")
    return environment


def host_tool(name: str) -> Path:
    folder = "file_to_c" if name == "file_to_c" else "n64recomp"
    return HOST_DIR / folder / f"{name}{EXE}"


def command_host(arguments: argparse.Namespace) -> None:
    step("Host tools (N64Recomp, RSPRecomp, file_to_c)")
    environment = host_environment()
    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"]
    if os.name == "nt":
        # Stand-alone programs: the Switch build runs them from another shell, without this PATH.
        common.append("-DCMAKE_EXE_LINKER_FLAGS=-static")

    recomp = ROOT / "lib" / "N64ModernRuntime" / "N64Recomp"
    run(["cmake", "-S", recomp, "-B", HOST_DIR / "n64recomp", *common,
         # A library of the recompiler relies on headers that newer standard libraries no longer pull in.
         "-DCMAKE_CXX_FLAGS=-include cstdlib -include cstdint -include cstring"], env=environment)
    run(["cmake", "--build", HOST_DIR / "n64recomp", "--target", "N64RecompCLI", "RSPRecomp"], env=environment)

    file_to_c = ROOT / "lib" / "rt64" / "src" / "tools" / "file_to_c"
    run(["cmake", "-S", file_to_c, "-B", HOST_DIR / "file_to_c", *common], env=environment)
    run(["cmake", "--build", HOST_DIR / "file_to_c"], env=environment)


# ---- generated code


def check_rom(rom: Path) -> None:
    if not rom.is_file():
        raise BuildError(f"ROM not found: {rom}")
    digest = hashlib.sha1(rom.read_bytes()).hexdigest()
    if digest != ROM_SHA1:
        raise BuildError(f"{rom.name} is not the supported ROM.\n  expected SHA-1 {ROM_SHA1} "
                         f"(Bomberman Hero, USA, .z64)\n  found    SHA-1 {digest}")


def makefile_variable(text: str, name: str) -> list[str]:
    match = re.search(rf"^{name}\s*:=\s*((?:.*\\\n)*.*)$", text, re.MULTILINE)
    if match is None:
        raise BuildError(f"patches/Makefile has no {name}")
    return match.group(1).replace("\\\n", " ").split()


def build_game_patches(environment: dict) -> None:
    """What patches/Makefile does, with its own flags, without needing make."""
    patches = ROOT / "patches"
    makefile = (patches / "Makefile").read_text(encoding="utf-8")
    compile_flags = makefile_variable(makefile, "CFLAGS") + makefile_variable(makefile, "CPPFLAGS") + PATCH_WARNINGS
    link_flags = makefile_variable(makefile, "LDFLAGS")

    objects = []
    for source in sorted(patches.glob("*.c")):
        output = source.with_suffix(".o")
        objects.append(output.name)
        if output.exists() and output.stat().st_mtime >= max(
                path.stat().st_mtime for path in [source, *patches.glob("*.h"), patches / "Makefile"]):
            continue
        run(["clang", *compile_flags, source.name, "-c", "-o", output.name], cwd=patches, env=environment, quiet=True)
        print(f"  compiled patches/{source.name}")

    # On Windows ld.lld only links ELF files when told which kind.
    run(["ld.lld", "-m", "elf32btsmip", *objects, *link_flags, "-o", "patches.elf"], cwd=patches, env=environment,
        quiet=True)
    print("  linked patches/patches.elf")


def command_recomp(arguments: argparse.Namespace) -> None:
    step("Generated code (from your ROM)")
    rom = Path(arguments.rom).resolve()
    check_rom(rom)
    for tool in ("N64Recomp", "RSPRecomp"):
        if not host_tool(tool).exists():
            raise BuildError("the host tools are missing: python tools/build.py host")

    local_rom = ROOT / ROM_NAME
    if not local_rom.exists() or local_rom.read_bytes() != rom.read_bytes():
        shutil.copyfile(rom, local_rom)

    shutil.rmtree(ROOT / "RecompiledFuncs", ignore_errors=True)
    (ROOT / "rsp").mkdir(exist_ok=True)
    run([host_tool("N64Recomp"), "bmhero.toml"], quiet=True)
    run([host_tool("RSPRecomp"), "aspMain.toml"], quiet=True)
    print(f"  RecompiledFuncs/: {len(list((ROOT / 'RecompiledFuncs').glob('*.c')))} files, rsp/n_aspMain.cpp")

    build_game_patches(host_environment())
    run([host_tool("N64Recomp"), "patches.toml"], quiet=True)
    print("  RecompiledPatches/patches.c, patches/patches.bin")


# ---- Switch build


def devkitpro_root() -> Path:
    """devkitPro's folder. On Windows the DEVKITPRO variable holds an MSYS2 path, so the usual places are tried."""
    candidates = [os.environ.get("BMHERO_DEVKITPRO"), os.environ.get("DEVKITPRO"), "C:/devkitPro",
                  str(Path.home() / "Downloads" / "devkitPro"), "/opt/devkitpro"]
    for candidate in candidates:
        if candidate and (Path(candidate) / "devkitA64").is_dir():
            return Path(candidate)
    raise BuildError("devkitPro was not found. Set BMHERO_DEVKITPRO to its folder (the one holding devkitA64).")


def devkitpro_tool(name: str) -> Path:
    return devkitpro_root() / "devkitA64" / "bin" / f"{name}{EXE}"


def devkitpro_shell() -> list[str] | None:
    """On Windows devkitPro's CMake toolchain only runs inside its MSYS2. Returns the command prefix for it."""
    if os.name != "nt":
        return None
    bash = devkitpro_root() / "msys2" / "usr" / "bin" / "bash.exe"
    if not bash.exists():
        raise BuildError(f"devkitPro's MSYS2 was not found ({bash}).")
    return [str(bash), "-lc"]


def run_switch(command: str) -> None:
    """Runs a shell command from the repository root, where the devkitA64 toolchain is available."""
    shell = devkitpro_shell()
    print(f"  $ {command}", flush=True)
    if shell is None:
        if not os.environ.get("DEVKITPRO"):
            raise BuildError("DEVKITPRO is not set. Install devkitA64 and libnx (docs/building.md).")
        result = subprocess.run(command, cwd=ROOT, shell=True)
    else:
        # A login shell that stays in this folder; relative paths keep Windows and MSYS2 in agreement.
        environment = dict(os.environ, MSYSTEM="MSYS", CHERE_INVOKING="1")
        result = subprocess.run([*shell, command], cwd=ROOT, env=environment)
    if result.returncode != 0:
        raise BuildError(f"command failed ({result.returncode}): {command}")


def command_nro(arguments: argparse.Namespace) -> None:
    step("Switch build (devkitA64)")
    required = {
        SDK_DIR / "lib" / "libvulkan.a": "the Vulkan driver SDK is missing: python tools/build.py sdk <zip>",
        host_tool("file_to_c"): "the host tools are missing: python tools/build.py host",
        ROOT / "RecompiledFuncs" / "funcs.h": "the generated code is missing: python tools/build.py recomp <rom>",
        ROOT / "RecompiledPatches" / "patches.c": "the generated code is missing: python tools/build.py recomp <rom>",
    }
    for path, message in required.items():
        if not path.exists():
            raise BuildError(message)

    if arguments.clean:
        shutil.rmtree(SWITCH_DIR, ignore_errors=True)
    if not (SWITCH_DIR / "build.ninja").exists():
        run_switch(
            'cmake -S . -B build/switch -G Ninja -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" '
            "-DCMAKE_BUILD_TYPE=Release "
            # Both features write absolute paths into files, where MSYS2 cannot translate them for the compiler.
            "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF")
    jobs = f" -j{arguments.jobs}" if arguments.jobs else ""
    run_switch(f"cmake --build build/switch --target BMHeroRecompiled_nro --{jobs}")
    print(f"  {(SWITCH_DIR / 'BombermanHeroNX.nro').relative_to(ROOT)}")


# ---- SD card


def command_sd(arguments: argparse.Namespace) -> None:
    step("SD card layout")
    nro = SWITCH_DIR / "BombermanHeroNX.nro"
    if not nro.exists():
        raise BuildError("the NRO is missing: python tools/build.py nro")

    SD_APP_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(nro, SD_APP_DIR / nro.name)
    shutil.copyfile(ROOT / "recompcontrollerdb.txt", SD_APP_DIR / "recompcontrollerdb.txt")
    shutil.rmtree(SD_APP_DIR / "assets", ignore_errors=True)
    shutil.copytree(ROOT / "assets", SD_APP_DIR / "assets")

    if arguments.rom:
        rom = Path(arguments.rom).resolve()
        check_rom(rom)
        shutil.copyfile(rom, SD_APP_DIR / ROM_NAME)

    size = sum(path.stat().st_size for path in SD_APP_DIR.rglob("*") if path.is_file())
    print(f"  {SD_APP_DIR.relative_to(ROOT)}: {size / 1e6:.1f} MB"
          + ("" if (SD_APP_DIR / ROM_NAME).exists() else f" (add your ROM as {ROM_NAME})"))


# ---- release archive


def project_version() -> str:
    text = (ROOT / "switch" / "cmake" / "SwitchTarget.cmake").read_text(encoding="utf-8")
    match = re.search(r'set\(BMHERO_SWITCH_VERSION "([^"]+)"', text)
    if match is None:
        raise BuildError("switch/cmake/SwitchTarget.cmake has no BMHERO_SWITCH_VERSION")
    return match.group(1)


def command_release(arguments: argparse.Namespace) -> None:
    """Zips the build that is in SD_CARD/, the one that was tested, and nothing a player has to supply."""
    step("Release archive")
    if not (SD_APP_DIR / "BombermanHeroNX.nro").exists():
        raise BuildError("SD_CARD/ is empty: python tools/build.py sd")

    # Named one by one: whatever else sits in the folder (a ROM, saves, logs) must never be published.
    files = [SD_APP_DIR / "BombermanHeroNX.nro", SD_APP_DIR / "recompcontrollerdb.txt"]
    files += sorted(path for path in (SD_APP_DIR / "assets").rglob("*") if path.is_file())

    archive_path = ROOT / "build" / f"BombermanHeroNX-{project_version()}.zip"
    archive_path.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files:
            archive.write(path, path.relative_to(ROOT / "SD_CARD").as_posix())
    print(f"  {archive_path.relative_to(ROOT)}: {len(files)} files, {archive_path.stat().st_size / 1e6:.1f} MB")


# ---- Vulkan driver SDK


def command_sdk(arguments: argparse.Namespace) -> None:
    step("Vulkan driver SDK")
    source = Path(arguments.archive).resolve()
    prefix = "opt/devkitpro/portlibs/switch/"
    wanted = ("include/vulkan/", "include/vk_video/", "lib/libvulkan.a", "share/")
    if not zipfile.is_zipfile(source):
        raise BuildError(f"{source} is not the SDK's zip file")

    shutil.rmtree(SDK_DIR, ignore_errors=True)
    with zipfile.ZipFile(source) as archive:
        for entry in archive.infolist():
            name = entry.filename
            if entry.is_dir() or not name.startswith(prefix) or not name[len(prefix):].startswith(wanted):
                continue
            target = SDK_DIR / name[len(prefix):]
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as data, open(target, "wb") as output:
                shutil.copyfileobj(data, output)
    library = SDK_DIR / "lib" / "libvulkan.a"
    if not library.exists():
        raise BuildError("the zip has no opt/devkitpro/portlibs/switch/lib/libvulkan.a")

    # The archive's symbol index leaves out the Rust runtime its shader compiler needs, so the linker cannot find
    # it even though it is inside. Rebuilding the index is enough.
    run([devkitpro_tool("aarch64-none-elf-ranlib"), library])
    print(f"  {SDK_DIR.relative_to(ROOT)}")


def command_all(arguments: argparse.Namespace) -> None:
    command_deps(argparse.Namespace(refresh=False))
    command_host(arguments)
    command_recomp(arguments)
    command_nro(arguments)
    command_sd(arguments)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)

    sub = commands.add_parser("deps", help="apply switch/patches to the dependencies")
    sub.add_argument("--refresh", action="store_true", help="rewrite the patches from lib/ instead")
    sub.set_defaults(function=command_deps)

    commands.add_parser("host", help="build the host tools").set_defaults(function=command_host)

    sub = commands.add_parser("recomp", help="generate the game's C code from the ROM")
    sub.add_argument("rom")
    sub.set_defaults(function=command_recomp)

    sub = commands.add_parser("nro", help="cross-compile the NRO")
    sub.set_defaults(function=command_nro)

    sub = commands.add_parser("sd", help="lay out SD_CARD/")
    sub.add_argument("--rom", help="also copy your ROM into the layout")
    sub.set_defaults(function=command_sd)

    commands.add_parser("release", help="zip SD_CARD/ for a release, without the ROM").set_defaults(
        function=command_release)

    sub = commands.add_parser("sdk", help="unpack the Vulkan driver SDK")
    sub.add_argument("archive")
    sub.set_defaults(function=command_sdk)

    sub = commands.add_parser("all", help="every step, in order")
    sub.add_argument("rom")
    sub.set_defaults(function=command_all)

    for name in ("nro", "all"):
        commands.choices[name].add_argument("--jobs", type=int, help="parallel compile jobs")
        commands.choices[name].add_argument("--clean", action="store_true", help="start the Switch build over")

    arguments = parser.parse_args()
    try:
        arguments.function(arguments)
    except BuildError as error:
        print(f"\nerror: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
