// The measurement hooks the renderer's Switch code calls: the frame rate shown on screen and written to the
// log, and the per-stage timings ("profile:" and "traffic:" lines) used to find what a slow frame spent time on.
//
// The stage collector follows the one in snowboardkids2-recomp-switch (GPL-3.0), with a longer report window.
#include "nx_platform.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {
    using Clock = std::chrono::steady_clock;

    constexpr auto FPS_DISPLAY_WINDOW = std::chrono::milliseconds(500);
    constexpr auto FPS_LOG_WINDOW = std::chrono::seconds(5);
    constexpr auto PROFILE_WINDOW = std::chrono::seconds(5);
    constexpr uint64_t FRAME_BUDGET_NS = 16'666'667;

    std::atomic<float> display_fps{ 0.0f };

    bool profile_enabled() {
        static const bool enabled = [] {
            const char *value = std::getenv("RECOMP_SWITCH_PROFILE");
            return value == nullptr || *value != '0';
        }();
        return enabled;
    }

    // 250 microsecond buckets with an overflow bucket; a percentile reports its bucket's upper bound.
    struct Histogram {
        std::array<uint32_t, 257> buckets{};
        uint64_t count = 0, total_ns = 0, max_ns = 0, late = 0;

        void add(uint64_t ns) {
            buckets[std::min<uint64_t>(ns / 250'000, buckets.size() - 1)]++;
            count++;
            total_ns += ns;
            max_ns = std::max(max_ns, ns);
            late += ns > FRAME_BUDGET_NS;
        }

        uint64_t percentile_ns(unsigned percent) const {
            if (count == 0 || max_ns == 0) {
                return 0;
            }
            const uint64_t target = (count * percent + 99) / 100;
            uint64_t accumulated = 0;
            for (size_t i = 0; i < buckets.size(); i++) {
                accumulated += buckets[i];
                if (accumulated >= target) {
                    return i == buckets.size() - 1 ? max_ns : (i + 1) * 250'000;
                }
            }
            return max_ns;
        }
    };

    struct Stage {
        const char *name = nullptr;
        Histogram stats;
    };

    struct Traffic {
        const char *name = nullptr;
        uint64_t count = 0, bytes = 0;
    };

    void collect(const char *stage, uint64_t value, bool traffic) {
        if (!profile_enabled()) {
            return;
        }

        static std::array<Stage, 48> stages;
        static std::array<Traffic, 16> transfers;
        static std::array<Stage, 48> stage_report;
        static std::array<Traffic, 16> transfer_report;
        static std::mutex collect_mutex, report_mutex;
        static auto last_report = Clock::now();

        // A finished window is copied under the collection lock and written outside of it, so that formatting and
        // file writes never hold up the audio, display list or GPU threads that call in here.
        std::unique_lock report_lock(report_mutex, std::defer_lock);
        double window_seconds = 0.0;
        {
            const std::lock_guard lock(collect_mutex);
            if (traffic) {
                for (Traffic &entry : transfers) {
                    if (entry.name == nullptr) entry.name = stage;
                    if (std::strcmp(entry.name, stage) == 0) {
                        entry.count++;
                        entry.bytes += value;
                        break;
                    }
                }
            }
            else {
                for (Stage &entry : stages) {
                    if (entry.name == nullptr) entry.name = stage;
                    if (std::strcmp(entry.name, stage) == 0) {
                        entry.stats.add(value);
                        break;
                    }
                }
            }

            const auto now = Clock::now();
            if (now - last_report < PROFILE_WINDOW || !report_lock.try_lock()) {
                return;
            }
            window_seconds = std::chrono::duration<double>(now - last_report).count();
            stage_report = stages;
            transfer_report = transfers;
            for (Stage &entry : stages) entry.stats = {};
            for (Traffic &entry : transfers) entry.count = entry.bytes = 0;
            last_report = now;
        }

        char line[384];
        for (const Stage &entry : stage_report) {
            const Histogram &h = entry.stats;
            if (h.count == 0) {
                continue;
            }
            std::snprintf(line, sizeof(line),
                "profile: %-28s n=%-5llu mean=%-6llu p50=%-6llu p95=%-6llu p99=%-6llu max=%-7llu us, over 16.6 ms: %llu",
                entry.name, static_cast<unsigned long long>(h.count),
                static_cast<unsigned long long>(h.total_ns / h.count / 1000),
                static_cast<unsigned long long>(h.percentile_ns(50) / 1000),
                static_cast<unsigned long long>(h.percentile_ns(95) / 1000),
                static_cast<unsigned long long>(h.percentile_ns(99) / 1000),
                static_cast<unsigned long long>(h.max_ns / 1000),
                static_cast<unsigned long long>(h.late));
            switch_log_checkpoint(line);
        }
        for (const Traffic &entry : transfer_report) {
            if (entry.count == 0) {
                continue;
            }
            std::snprintf(line, sizeof(line), "traffic: %-28s n=%-5llu %.1f KB/s", entry.name,
                static_cast<unsigned long long>(entry.count), entry.bytes / 1024.0 / window_seconds);
            switch_log_checkpoint(line);
        }
    }
}

extern "C" {
    float switch_fps_value() {
        return display_fps.load(std::memory_order_relaxed);
    }

    // Called once for every image handed to the screen, interpolated frames included.
    void switch_fps_presented() {
        static auto display_start = Clock::time_point{};
        static auto log_start = Clock::time_point{};
        static auto previous = Clock::time_point{};
        static uint32_t display_frames = 0, log_frames = 0, late_frames = 0;
        static uint64_t worst_ns = 0;

        const auto now = Clock::now();
        if (previous == Clock::time_point{}) {
            display_start = log_start = previous = now;
            return;
        }

        const uint64_t frame_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous).count();
        previous = now;
        worst_ns = std::max(worst_ns, frame_ns);
        late_frames += frame_ns > FRAME_BUDGET_NS * 5 / 4;
        display_frames++;
        log_frames++;

        if (now - display_start >= FPS_DISPLAY_WINDOW) {
            display_fps.store(float(display_frames / std::chrono::duration<double>(now - display_start).count()),
                std::memory_order_relaxed);
            display_frames = 0;
            display_start = now;
        }

        if (now - log_start >= FPS_LOG_WINDOW) {
            char line[160];
            std::snprintf(line, sizeof(line), "frames: %.1f fps, worst %.1f ms, %u of %u late",
                log_frames / std::chrono::duration<double>(now - log_start).count(), worst_ns / 1e6,
                late_frames, log_frames);
            switch_log_checkpoint(line);
            log_frames = late_frames = 0;
            worst_ns = 0;
            log_start = now;
        }
    }

    void switch_perf_record(const char *stage, uint64_t elapsed_ns, uint64_t id) {
        (void)id;
        collect(stage, elapsed_ns, false);
    }

    void switch_perf_record_bytes(const char *stage, uint64_t bytes, uint64_t id) {
        (void)id;
        collect(stage, bytes, true);
    }

    // Frame captures over the network belong to the reference port's test rig; nothing asks for one here.
    bool switch_screenshot_capture_requested() { return false; }
    bool switch_manual_screenshot_enabled() { return false; }
    bool switch_request_manual_screenshot() { return false; }
    void switch_screenshot_capture_complete(const void *pixels, size_t size, uint32_t width, uint32_t height) {
        (void)pixels;
        (void)size;
        (void)width;
        (void)height;
    }
}
