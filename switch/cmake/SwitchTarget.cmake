# Nintendo Switch: the platform layer, the Vulkan driver and the NRO. Included at the end of the root
# CMakeLists.txt, once the game's executable and its libraries are defined.

set(BMHERO_SWITCH_VERSION "0.1.0" CACHE STRING "Version shown in the homebrew menu")
set(BMHERO_SWITCH_SOURCE_DIR "${CMAKE_SOURCE_DIR}/switch/src")

target_sources(BMHeroRecompiled PRIVATE
    ${BMHERO_SWITCH_SOURCE_DIR}/nx_main.cpp
    ${BMHERO_SWITCH_SOURCE_DIR}/nx_log.cpp
    ${BMHERO_SWITCH_SOURCE_DIR}/nx_crash.cpp
    ${BMHERO_SWITCH_SOURCE_DIR}/nx_profile.cpp
    ${BMHERO_SWITCH_SOURCE_DIR}/mman.c
    ${BMHERO_SWITCH_SOURCE_DIR}/egl_stubs.c
    # Defines SDL's audio backend before libSDL2.a is searched, so the library's own is left out.
    ${BMHERO_SWITCH_SOURCE_DIR}/sdl_audio/SDL_switchaudio.c
)

target_include_directories(BMHeroRecompiled PRIVATE
    ${BMHERO_SWITCH_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)

target_link_libraries(BMHeroRecompiled PRIVATE
    SDL2::SDL2
    PkgConfig::FREETYPE
    SwitchNVK
)

set_target_properties(BMHeroRecompiled PROPERTIES OUTPUT_NAME "BombermanHeroNX")

set(BMHERO_SWITCH_NACP "${CMAKE_BINARY_DIR}/BombermanHeroNX.nacp")
nx_generate_nacp(
    OUTPUT "${BMHERO_SWITCH_NACP}"
    NAME "Bomberman Hero"
    AUTHOR "hazevauks"
    VERSION "${BMHERO_SWITCH_VERSION}"
)
nx_create_nro(
    BMHeroRecompiled
    OUTPUT "${CMAKE_BINARY_DIR}/BombermanHeroNX.nro"
    ICON "${CMAKE_SOURCE_DIR}/switch/icon.jpg"
    NACP "${BMHERO_SWITCH_NACP}"
)
