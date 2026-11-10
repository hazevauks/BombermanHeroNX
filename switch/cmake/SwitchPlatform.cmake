# Nintendo Switch (Horizon) platform setup. Included from the root CMakeLists.txt before any dependency is added,
# when the build uses devkitPro's Switch.cmake toolchain.
#
# It provides what the desktop build finds on the host: SDL2 and FreeType from devkitPro's portlibs, the Vulkan
# driver (Mesa's NVK, a static library linked into the NRO), and the host tools a cross build cannot build for itself.

set(BMHERO_SWITCH_APP_FOLDER "sdmc:/switch/BombermanHeroNX" CACHE STRING "Application folder on the SD card")

# ---- host tools (tools/build.py builds them before configuring)

set(BMHERO_HOST_TOOLS_DIR "${CMAKE_SOURCE_DIR}/build/host" CACHE PATH "Folder with the tools built for the host")

if (CMAKE_HOST_WIN32)
    set(BMHERO_HOST_EXE ".exe")
else()
    set(BMHERO_HOST_EXE "")
endif()

set(RECOMP_HOST_FILE_TO_C "${BMHERO_HOST_TOOLS_DIR}/file_to_c/file_to_c${BMHERO_HOST_EXE}"
    CACHE FILEPATH "file_to_c built for the host")
if (NOT EXISTS "${RECOMP_HOST_FILE_TO_C}")
    message(FATAL_ERROR "The host tools are missing (${RECOMP_HOST_FILE_TO_C}). Run: python tools/build.py host")
endif()

# ---- libraries from devkitPro's portlibs

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

# SDL2's pkg-config file also links devkitPro's OpenGL Mesa (EGL, glapi, drm_nouveau): an older copy of the code
# the Vulkan driver is built from, which must not end up in the same program. SDL only needs it to open a window,
# which the game never does; switch/src/egl_stubs.c answers the symbols instead.
set_target_properties(PkgConfig::SDL2 PROPERTIES INTERFACE_LINK_LIBRARIES "${SDL2_LIBDIR}/libSDL2.a;nx")

if (NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()
set(SDL2_FOUND TRUE)
set(SDL2_LIBRARIES SDL2::SDL2)

# ---- Vulkan: Mesa NVK for Horizon, as a static library

set(BMHERO_SWITCH_NVK_SDK "${CMAKE_SOURCE_DIR}/third_party/nvk-sdk" CACHE PATH
    "portlibs/switch folder of the Mesa Horizon SDK (include/vulkan and lib/libvulkan.a)")
if (NOT EXISTS "${BMHERO_SWITCH_NVK_SDK}/lib/libvulkan.a" OR
    NOT EXISTS "${BMHERO_SWITCH_NVK_SDK}/include/vulkan/vulkan.h")
    message(FATAL_ERROR
        "The Vulkan driver SDK was not found in '${BMHERO_SWITCH_NVK_SDK}'.\n"
        "See docs/building.md: unpack the Mesa Horizon SDK and run python tools/build.py sdk <zip>.")
endif()

# The driver has no loader: the program starts from its ICD entry points, which nothing else references, hence
# the -u options. Its Rust parts each carry their own copy of the Rust runtime, hence
# --allow-multiple-definition.
add_library(SwitchNVK INTERFACE)
target_include_directories(SwitchNVK SYSTEM INTERFACE "${BMHERO_SWITCH_NVK_SDK}/include")
target_link_libraries(SwitchNVK INTERFACE
    "-Wl,--start-group"
    "${BMHERO_SWITCH_NVK_SDK}/lib/libvulkan.a" expat zstd z nx stdc++ m
    "-Wl,--end-group")
target_link_options(SwitchNVK INTERFACE
    "-Wl,-u,vk_icdGetInstanceProcAddr"
    "-Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion"
    "-Wl,--allow-multiple-definition"
    "-pthread")

# The driver's headers come first: portlibs may hold another, older set.
include_directories(BEFORE SYSTEM "${BMHERO_SWITCH_NVK_SDK}/include")

# ---- definitions and headers shared by the game and its dependencies

add_compile_definitions(
    _GNU_SOURCE
    RECOMP_SWITCH_APP_FOLDER="${BMHERO_SWITCH_APP_FOLDER}"
    PLUME_SDL_VULKAN_ENABLED
    RT64_SDL_WINDOW_VULKAN
    VK_USE_PLATFORM_VI_NN
)

# POSIX headers newlib does not have.
include_directories(BEFORE "${CMAKE_SOURCE_DIR}/switch/compat")

# The shaders are compiled on the host; these mirror the options the desktop build sets after its dependencies.
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "-I${CMAKE_SOURCE_DIR}/src" "-E" "PSMain" "-T" "ps_6_0" "-D" "DYNAMIC_RENDER_PARAMS")
set(DXC_VS_OPTS "-I${CMAKE_SOURCE_DIR}/src" "-E" "VSMain" "-T" "vs_6_0" "-D" "DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
