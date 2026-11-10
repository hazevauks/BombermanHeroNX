// Keeps the audio queue from growing without dropping samples.
//
// The game produces sound in chunks, once per frame. When frames run late the chunks pile up in SDL's queue and
// the sound falls behind the picture. The desktop build answers by throwing away every other sample of a chunk,
// which is audible. Here the backlog is played back very slightly faster instead (at most 5%), with linear
// interpolation and the phase carried from one chunk to the next, so catching up cannot be heard as a click.
//
// From snowboardkids2-recomp-switch (GPL-3.0).
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nx::audio {
    // The queue's bytes are in the output device's format, not in the game's.
    constexpr uint64_t queued_microseconds(uint64_t bytes, uint32_t output_rate, uint32_t channels) {
        return output_rate != 0 && channels != 0
            ? (bytes / (uint64_t(channels) * sizeof(float))) * 1'000'000 / output_rate
            : 0;
    }

    // Stereo float samples in, stereo float samples out.
    class QueueCorrector {
        std::array<float, 2> previous{};
        double position = 0.0;
        double rate = 0.0;
        bool has_previous = false;
        bool catching_up = false;

    public:
        void reset() { *this = QueueCorrector{}; }

        void process(const float *input, size_t frames, uint64_t queued_us, std::vector<float> &output) {
            output.clear();
            if (frames == 0) {
                return;
            }

            // Start above 110 ms of backlog, stop below 80 ms, and speed up more the further behind it is.
            if (queued_us > 110'000) {
                catching_up = true;
            }
            else if (queued_us < 80'000) {
                catching_up = false;
            }
            const double target = !catching_up ? 0.0 : queued_us > 500'000 ? 0.05 : queued_us > 250'000 ? 0.02 : 0.005;

            const size_t prefix = has_previous ? 1 : 0;
            const size_t available = frames + prefix;
            output.reserve(available * 2);
            while (position < double(available - 1)) {
                const size_t first = size_t(position);
                const float fraction = float(position - double(first));
                for (size_t channel = 0; channel < 2; channel++) {
                    const float a = prefix != 0 && first == 0 ? previous[channel] : input[(first - prefix) * 2 + channel];
                    const float b = input[(first + 1 - prefix) * 2 + channel];
                    output.push_back(a + (b - a) * fraction);
                }
                // The speed itself changes slowly, so the pitch never jumps.
                rate += std::clamp(target - rate, -0.0000005, 0.0000005);
                position += 1.0 + rate;
            }
            position -= double(available - 1);
            previous = { input[(frames - 1) * 2], input[(frames - 1) * 2 + 1] };
            has_previous = true;
        }
    };
}
