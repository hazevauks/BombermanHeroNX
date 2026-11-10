// Startup and shutdown of the Switch platform layer: how the game was started, the SD card layout, the settings
// the renderer and the driver read from the environment, and the fatal error paths.
#include "nx_platform.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <sys/stat.h>

#include <switch.h>

namespace nx {
    void log_open();
    void log_close();
}

namespace {
    const char *const app_folders[] = {
        NX_APP_DIR,
        NX_APP_DIR "/config",
        NX_APP_DIR "/saves",
        NX_APP_DIR "/mods",
        NX_APP_DIR "/cache",
        NX_LOG_DIR,
    };

    // What the renderer's Switch code paths read with getenv(). The values are the ones that tested best on
    // hardware for this renderer; a flag file in <app folder>/config flips the ones that are worth trying.
    struct Setting {
        const char *variable;
        const char *value; // nullptr: leave the variable unset
        const char *flag_file; // present: use `flagged_value` instead
        const char *flagged_value;
    };

    const Setting settings[] = {
        // Mesa only offers the Tegra X1's GPU to applications that ask for it.
        { "NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", nullptr, nullptr },
        // Framebuffer contents the game reads back are packed on the GPU and moved in one transfer.
        { "RECOMP_SWITCH_PACKED_COPYBACK", "1", "rgba-copyback", "0" },
        { "RECOMP_SWITCH_FUSED_RAM_TRANSFERS", "1", "staged-transfers", "0" },
        // The graphics menu, not a fixed diagnostic size, chooses the resolution.
        { "RECOMP_SWITCH_DIAGNOSTIC_NATIVE_RESOLUTION", "1", nullptr, nullptr },
        // On while the port is being tuned: the frame rate on screen and stage timings in the log.
        { "RECOMP_SWITCH_FPS", "1", "no-fps", "0" },
        { "RECOMP_SWITCH_PROFILE", "1", "no-profile", "0" },
        // The driver's slower, more conservative paths, to tell a driver problem from a renderer problem:
        // waiting on native fences, and command memory the CPU reads back through its cache.
        { "NVK_SWITCH_MAPPED_COMPLETION", nullptr, "nvk-native-completion", "0" },
        { "NVK_SWITCH_CMD_MEM_CPU_UNCACHED", nullptr, "nvk-cached-memory", "0" },
        { "NVK_SWITCH_MEM_STREAM_CPU_UNCACHED", nullptr, "nvk-cached-memory", "0" },
    };

    bool flag_exists(const char *name) {
        char path[256];
        snprintf(path, sizeof(path), NX_APP_DIR "/config/%s", name);
        struct stat info;
        return stat(path, &info) == 0;
    }

    bool started_as_application() {
        const AppletType type = appletGetAppletType();
        return type == AppletType_Application || type == AppletType_SystemApplication;
    }

    // Before the renderer owns the screen, messages can use libnx's text console.
    void show_on_console(const char *text) {
        consoleInit(nullptr);
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        PadState pad;
        padInitializeDefault(&pad);

        printf("\n\n  Bomberman Hero\n\n%s\n\n  Press + to go back.\n", text);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & (HidNpadButton_Plus | HidNpadButton_B)) {
                break;
            }
            consoleUpdate(nullptr);
        }
        consoleExit(nullptr);
    }

    u32 clock_hz(PcvModuleId module) {
        u32 hz = 0;
        ClkrstSession session;
        if (R_SUCCEEDED(clkrstOpenSession(&session, module, 3))) {
            clkrstGetClockRate(&session, &hz);
            clkrstCloseSession(&session);
        }
        return hz;
    }

    void log_system() {
        const u32 version = hosversionGet();
        nx::log("system: Horizon %u.%u.%u%s, %s", HOSVER_MAJOR(version), HOSVER_MINOR(version), HOSVER_MICRO(version),
            hosversionIsAtmosphere() ? " (Atmosphere)" : "",
            appletGetOperationMode() == AppletOperationMode_Console ? "docked" : "handheld");

        u64 total = 0, used = 0;
        svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
        nx::log("memory: %llu MB for the process, %llu MB in use at startup",
            static_cast<unsigned long long>(total >> 20), static_cast<unsigned long long>(used >> 20));

        // The clock service that can be read without changing anything exists from Horizon 8.0.0.
        if (hosversionAtLeast(8, 0, 0) && R_SUCCEEDED(clkrstInitialize())) {
            nx::log("clocks: CPU %.1f MHz, GPU %.1f MHz, memory %.1f MHz",
                clock_hz(PcvModuleId_CpuBus) / 1e6, clock_hz(PcvModuleId_GPU) / 1e6, clock_hz(PcvModuleId_EMC) / 1e6);
            clkrstExit();
        }
    }

    void apply_settings() {
        for (const Setting &setting : settings) {
            const bool flagged = setting.flag_file != nullptr && flag_exists(setting.flag_file);
            const char *value = flagged ? setting.flagged_value : setting.value;
            if (value == nullptr) {
                continue;
            }
            setenv(setting.variable, value, 1);
            if (flagged) {
                nx::log("config: %s present, %s=%s", setting.flag_file, setting.variable, value);
            }
        }
    }

    void on_terminate() {
        const std::exception_ptr active = std::current_exception();
        if (active != nullptr) {
            try {
                std::rethrow_exception(active);
            }
            catch (const std::exception &exception) {
                nx::fatal("Unhandled C++ exception:\n%s", exception.what());
            }
            catch (...) {
                nx::fatal("Unhandled C++ exception of an unknown type.");
            }
        }
        nx::fatal("std::terminate was called without an active exception.");
    }
}

namespace nx {
    bool startup(int argc, char **argv) {
        (void)argc;
        (void)argv;

        // From the album the homebrew menu runs as an applet, with a fraction of the memory the game needs.
        if (!started_as_application()) {
            show_on_console(
                "  The game needs the full memory of the console.\n\n"
                "  Start the homebrew menu from a game (hold R while opening\n"
                "  any game) or use a forwarder, then start Bomberman Hero again.");
            return false;
        }

        for (const char *folder : app_folders) {
            mkdir(folder, 0777);
        }

        log_open();
        log("startup: BombermanHeroNX, built " __DATE__ " " __TIME__);
        log_system();
        apply_settings();
        std::set_terminate(on_terminate);
        return true;
    }

    void shutdown() {
        log("shutdown: leaving cleanly");
        log_close();
    }

    bool pump_applet() {
        // SDL's event polling does not promise to drain libnx's applet messages (focus, sleep, home button).
        u32 message;
        while (R_SUCCEEDED(appletGetMessage(&message))) {
            if (!appletProcessMessage(message)) {
                return false;
            }
        }
        return true;
    }

    void fatal(const char *format, ...) {
        char message[1536];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        FILE *file = fopen(NX_FATAL_FILE, "w");
        if (file != nullptr) {
            fprintf(file, "=== BombermanHeroNX fatal error ===\n%s\n", message);
            fclose(file);
        }
        log("FATAL: %s", message);
        log_flush();

        // The error applet draws over whatever owns the screen.
        ErrorApplicationConfig config;
        if (R_SUCCEEDED(errorApplicationCreate(&config, "Bomberman Hero has stopped.", message))) {
            errorApplicationShow(&config);
        }
        std::_Exit(EXIT_FAILURE);
    }

    bool option(const char *name) {
        return flag_exists(name);
    }
}
