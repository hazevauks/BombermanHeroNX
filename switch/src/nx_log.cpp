// SD_LOG/bmhero_log.txt: everything the port and its libraries print, stdout and stderr included.
//
// The file is buffered and pushed to the card every couple of seconds, so logging costs little while the game
// runs; the fatal paths flush it themselves. Started with nxlink, the same lines also go to the PC.
#include "nx_platform.h"

#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

namespace {
    constexpr size_t LOG_BUFFER_SIZE = 64 * 1024;
    constexpr u64 FLUSH_INTERVAL_NS = 2'000'000'000ULL;

    std::mutex log_mutex;
    FILE *log_file = nullptr;
    u64 start_tick = 0;
    u64 last_flush_tick = 0;
    int nxlink_socket = -1;
    bool socket_started = false;

    void flush_locked() {
        if (log_file != nullptr) {
            fflush(log_file);
            fsync(fileno(log_file));
        }
        last_flush_tick = armGetSystemTick();
    }

    void write_locked(const char *line, size_t length) {
        if (log_file != nullptr) {
            fwrite(line, 1, length, log_file);
            if (armTicksToNs(armGetSystemTick() - last_flush_tick) >= FLUSH_INTERVAL_NS) {
                flush_locked();
            }
        }

        // Never block on the network: a line that does not fit is dropped.
        if (nxlink_socket >= 0) {
            const ssize_t sent = send(nxlink_socket, line, length, MSG_DONTWAIT | MSG_NOSIGNAL);
            if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                close(nxlink_socket);
                nxlink_socket = -1;
            }
        }
    }

    void write_timestamped(const char *message) {
        const size_t message_length = strlen(message);
        const bool has_newline = message_length > 0 && message[message_length - 1] == '\n';

        const std::lock_guard<std::mutex> lock(log_mutex);
        const u64 elapsed_ms = armTicksToNs(armGetSystemTick() - start_tick) / 1'000'000ULL;
        char line[1280];
        const int length = snprintf(line, sizeof(line), "[%8" PRIu64 " ms] %s%s", elapsed_ms, message, has_newline ? "" : "\n");
        if (length > 0) {
            write_locked(line, length < int(sizeof(line)) ? size_t(length) : sizeof(line) - 1);
        }
    }
}

namespace nx {
    void log_open() {
        const std::lock_guard<std::mutex> lock(log_mutex);
        start_tick = armGetSystemTick();
        last_flush_tick = start_tick;

        mkdir(NX_LOG_DIR, 0777);
        remove(NX_LOG_PREVIOUS_FILE);
        rename(NX_LOG_FILE, NX_LOG_PREVIOUS_FILE);

        log_file = fopen(NX_LOG_FILE, "w");
        if (log_file != nullptr) {
            setvbuf(log_file, nullptr, _IOFBF, LOG_BUFFER_SIZE);

            // The libraries print through stdio. Both streams share the log's file, each with its own buffer.
            fflush(stdout);
            fflush(stderr);
            dup2(fileno(log_file), STDOUT_FILENO);
            dup2(fileno(log_file), STDERR_FILENO);
            setvbuf(stdout, nullptr, _IOLBF, 0);
            setvbuf(stderr, nullptr, _IONBF, 0);
        }

        // hbloader only fills this address in when nxlink sent the NRO.
        if (__nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault())) {
            socket_started = true;
            nxlink_socket = nxlinkConnectToHost(false, false);
            if (nxlink_socket >= 0) {
                const int flags = fcntl(nxlink_socket, F_GETFL, 0);
                if (flags >= 0) {
                    fcntl(nxlink_socket, F_SETFL, flags | O_NONBLOCK);
                }
            }
        }
    }

    void log_close() {
        const std::lock_guard<std::mutex> lock(log_mutex);
        flush_locked();
        if (nxlink_socket >= 0) {
            close(nxlink_socket);
            nxlink_socket = -1;
        }
        if (socket_started) {
            socketExit();
            socket_started = false;
        }
    }

    void log(const char *format, ...) {
        char message[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        write_timestamped(message);
    }

    void log_flush() {
        const std::lock_guard<std::mutex> lock(log_mutex);
        flush_locked();
    }

    // For the crash handler: the faulting thread may be the one holding the lock.
    void log_flush_unlocked() {
        flush_locked();
    }
}

void switch_log_checkpoint(const char *message, bool reset) {
    if (message == nullptr) {
        return;
    }
    if (reset) {
        const std::lock_guard<std::mutex> lock(log_mutex);
        start_tick = armGetSystemTick();
    }
    write_timestamped(message);
}

// The name the renderer's patches use for driver messages.
extern "C" void dusk_switch_log(const char *message) {
    if (message != nullptr) {
        write_timestamped(message);
    }
}
