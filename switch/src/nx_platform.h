// The Switch platform layer: SD card layout, logging, crash reports and the hooks the patched dependencies call.
#pragma once

#include <cstdint>

#ifndef RECOMP_SWITCH_APP_FOLDER
#error "RECOMP_SWITCH_APP_FOLDER must name the application folder on the SD card"
#endif

#define NX_APP_DIR RECOMP_SWITCH_APP_FOLDER
#define NX_LOG_DIR NX_APP_DIR "/SD_LOG"
#define NX_LOG_FILE NX_LOG_DIR "/bmhero_log.txt"
#define NX_LOG_PREVIOUS_FILE NX_LOG_DIR "/bmhero_log_previous.txt"
#define NX_CRASH_FILE NX_LOG_DIR "/bmhero_crash.txt"
#define NX_FATAL_FILE NX_LOG_DIR "/bmhero_fatal.txt"

namespace nx {
    // Creates the folders the game expects, opens the log and installs the fatal error handlers.
    // Returns false when the game cannot run as it was started; the reason is already on screen.
    bool startup(int argc, char **argv);
    void shutdown();

    // Once per frame of the event loop: applet messages (focus, sleep, home button). False means "quit".
    bool pump_applet();

    void log(const char *format, ...) __attribute__((format(printf, 1, 2)));
    void log_flush();

    // Writes the message to bmhero_fatal.txt and to the log, then shows it and waits for a button.
    [[noreturn]] void fatal(const char *format, ...) __attribute__((format(printf, 1, 2)));

    // The flag files in <app folder>/config, read once at startup.
    bool option(const char *name);
}

// ---- hooks the patched dependencies call (lib/rt64, plume, RecompFrontend, N64ModernRuntime)

// A timestamped log line. `reset` restarts the clock the timestamps count from.
void switch_log_checkpoint(const char *message, bool reset = false);

extern "C" {
    float switch_fps_value();
    void switch_fps_presented();
    void switch_perf_record(const char *stage, uint64_t elapsed_ns, uint64_t id);
    void switch_perf_record_bytes(const char *stage, uint64_t bytes, uint64_t id);
}
