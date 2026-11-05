#include "rear_audio.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace {
constexpr unsigned ring_size = 1u << 18;
constexpr uint64_t settings_mask = (UINT64_C(1) << 47) - 1;
constexpr uint64_t defaults = UINT64_C(1) | (UINT64_C(100) << 1) |
    (UINT64_C(1) << 15) | (UINT64_C(100) << 16) | (UINT64_C(100) << 23) |
    (UINT64_C(1) << 40);
std::atomic<uint64_t> settings{defaults};
std::atomic<unsigned> meters[2];
std::atomic<unsigned> audio_users{0};
float gains[2][2]{};
struct Recording {
    float samples[ring_size][2];
    std::atomic<uint64_t> write{0}, read{0}, frames{0};
    std::atomic<unsigned> state{REAR_IDLE}, stop{0}, cancel{0}, failure{0};
    unsigned channels = 0, rate = 0;
    float threshold = 0;
    uint64_t limit = 0;
    char directory[1024]{}, paths[2][1100]{};
    std::thread worker;
    ~Recording() {
        cancel.store(1,std::memory_order_release);
        if(worker.joinable())worker.join();
    }
} recording;
enum { ERROR_NONE, ERROR_FILE, ERROR_OVERFLOW, ERROR_RATE, ERROR_EMPTY, ERROR_SOURCE };
float finite(float sample) { return std::isfinite(sample) ? sample : 0.f; }
void put(unsigned char *p, uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) p[i] = (unsigned char)(value >> (i * 8));
}
bool write_all(int fd, const void *data, size_t bytes) {
    const auto *p = static_cast<const unsigned char *>(data);
    while (bytes) {
        ssize_t n = write(fd, p, bytes);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        p += n;
        bytes -= (size_t)n;
    }
    return true;
}
bool finish_file(int fd, uint64_t frames, unsigned rate) {
    unsigned char header[44]{};
    std::memcpy(header, "RIFF", 4);
    uint64_t bytes = frames * 3;
    put(header + 4, (uint32_t)(36 + bytes + (bytes & 1)), 4);
    std::memcpy(header + 8, "WAVEfmt ", 8);
    put(header + 16, 16, 4);
    put(header + 20, 1, 2);
    put(header + 22, 1, 2);
    put(header + 24, rate, 4);
    put(header + 28, rate * 3, 4);
    put(header + 32, 3, 2);
    put(header + 34, 24, 2);
    std::memcpy(header + 36, "data", 4);
    put(header + 40, (uint32_t)bytes, 4);
    if (bytes & 1) { const unsigned char pad = 0; if (!write_all(fd,&pad,1)) return false; }
    return lseek(fd, 0, SEEK_SET) == 0 && write_all(fd, header, sizeof(header)) && fsync(fd) == 0;
}
void writer() {
    auto &r = recording;
    int fd[2] = {-1, -1};
    for (unsigned ch = 0; ch < 2; ++ch) {
        if (!(r.channels & (1u << ch))) continue;
        fd[ch] = open(r.paths[ch], O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        unsigned char empty[44]{};
        if (fd[ch] < 0 || !write_all(fd[ch], empty, sizeof(empty))) r.failure.store(ERROR_FILE);
    }
    r.state.store(r.failure.load() ? REAR_FINISHING : REAR_WAITING, std::memory_order_release);
    uint64_t saved = 0;
    while (!r.cancel.load() && !r.failure.load()) {
        uint64_t read = r.read.load(std::memory_order_relaxed);
        uint64_t end = r.write.load(std::memory_order_acquire);
        unsigned count = (unsigned)std::min<uint64_t>(end - read, 4096);
        if (!count) {
            if (r.stop.load(std::memory_order_acquire) == 2 && r.write.load(std::memory_order_acquire) == read) break;
            usleep(2000);
            continue;
        }
        unsigned char block[4096 * 3];
        for (unsigned ch = 0; ch < 2; ++ch) {
            if (fd[ch] < 0) continue;
            for (unsigned n = 0; n < count; ++n) {
                float value = std::max(-1.f, std::min(1.f, r.samples[(read + n) & (ring_size - 1)][ch]));
                int32_t pcm = (int32_t)std::lrint(value * 8388607.f);
                put(block + n * 3, (uint32_t)pcm, 3);
            }
            if (!write_all(fd[ch], block, count * 3)) r.failure.store(ERROR_FILE);
        }
        saved += count;
        r.read.store(read + count, std::memory_order_release);
    }
    r.state.store(REAR_FINISHING,std::memory_order_release);
    while(audio_users.load(std::memory_order_acquire))usleep(1000);
    if (!saved && !r.cancel.load() && !r.failure.load()) r.failure.store(ERROR_EMPTY);
    for (unsigned ch = 0; ch < 2; ++ch) {
        if (fd[ch] < 0) continue;
        if (!r.cancel.load() && !r.failure.load() && !finish_file(fd[ch], saved, r.rate))
            r.failure.store(ERROR_FILE);
        if (close(fd[ch]) != 0) r.failure.store(ERROR_FILE);
    }
    r.state.store(r.cancel.load() ? REAR_IDLE : r.failure.load() ? REAR_ERROR : REAR_READY,
                  std::memory_order_release);
}
}

extern "C" uint64_t rear_settings_snapshot() { return settings.load(std::memory_order_acquire); }
extern "C" int rear_settings_valid(uint64_t v) {
    if (v & ~settings_mask) return 0;
    for (unsigned ch = 0; ch < 2; ++ch)
        if (((v >> (ch * 15 + 1)) & 127) > 100 || ((v >> (ch * 15 + 8)) & 127) > 100) return 0;
    return 1;
}
extern "C" void rear_settings_restore(uint64_t v) { if (rear_settings_valid(v)) settings.store(v); }
extern "C" void rear_settings_reset() { settings.store(defaults); }
extern "C" unsigned rear_setting(unsigned ch, unsigned field) {
    if (ch > 1) return 0;
    uint64_t v = rear_settings_snapshot();
    switch (field) {
        case REAR_MONITOR: return (v >> (ch * 15)) & 1;
        case REAR_LEVEL: return (v >> (ch * 15 + 1)) & 127;
        case REAR_PAN: return (v >> (ch * 15 + 8)) & 127;
        case REAR_ENABLED: return (v >> 30) & 1;
        case REAR_MODE: return (v >> 31) & 3;
        case REAR_TARGET: return (v >> (33 + ch * 7)) & 127;
        default: return 0;
    }
}
extern "C" int rear_set_setting(unsigned ch, unsigned field, unsigned value) {
    if (ch > 1 || ((field == REAR_MODE || field == REAR_TARGET) && rear_record_state() != REAR_IDLE)) return 0;
    unsigned shift, width, max;
    switch (field) {
        case REAR_MONITOR: shift = ch * 15; width = 1; max = 1; break;
        case REAR_LEVEL: shift = ch * 15 + 1; width = 7; max = 100; break;
        case REAR_PAN: shift = ch * 15 + 8; width = 7; max = 100; break;
        case REAR_ENABLED: shift = 30; width = 1; max = 1; break;
        case REAR_MODE: shift = 31; width = 2; max = 3; break;
        case REAR_TARGET: shift = 33 + ch * 7; width = 7; max = 127; break;
        default: return 0;
    }
    if (value > max) return 0;
    uint64_t mask = ((UINT64_C(1) << width) - 1) << shift;
    uint64_t previous = settings.load();
    while (!settings.compare_exchange_weak(previous, (previous & ~mask) | (uint64_t(value) << shift))) {}
    return 1;
}
extern "C" void rear_audio_meters(float levels[2]) {
    for (unsigned ch = 0; ch < 2; ++ch) { unsigned value = meters[ch].load(); std::memcpy(levels + ch, &value, 4); }
}
extern "C" void rear_audio_process(const float *left, const float *right, float *out_left,
                                    float *out_right, unsigned frames, unsigned rate, int monitor) {
    if (!left || !right || !frames || frames > 8192) return;
    audio_users.fetch_add(1,std::memory_order_acquire);
    const float *input[2] = {left, right};
    uint64_t config = rear_settings_snapshot();
    float peak[2] = {}, target[2][2]{};
    for (unsigned ch = 0; ch < 2; ++ch) {
        float pan = float((config >> (ch * 15 + 8)) & 127) / 100.f;
        float level = float((config >> (ch * 15 + 1)) & 127) / 100.f;
        if (!monitor || !(config & (UINT64_C(1) << 30)) || !(config & (UINT64_C(1) << (ch * 15)))) level = 0;
        /* Equal-power pan with unity at the endpoints. Smooth every gain to
         * avoid clicks on mute, pan and level changes. Recorded PCM is dry. */
        target[ch][0] = level * std::cos(pan * 1.5707963267948966f);
        target[ch][1] = level * std::sin(pan * 1.5707963267948966f);
    }
    float smoothing = rate ? 1.f - std::exp(-1.f / (.005f * rate)) : 1.f;
    auto &r = recording;
    unsigned state = r.state.load(std::memory_order_acquire);
    bool capture = state == REAR_WAITING || state == REAR_RECORDING;
    uint64_t write = r.write.load(std::memory_order_relaxed);
    uint64_t read = r.read.load(std::memory_order_acquire);
    if (capture && rate != r.rate) { r.failure.store(ERROR_RATE); capture = false; }
    for (unsigned n = 0; n < frames; ++n) {
        float sample[2] = {finite(input[0][n]), finite(input[1][n])};
        for (unsigned ch = 0; ch < 2; ++ch) {
            peak[ch] = std::max(peak[ch], std::fabs(sample[ch]));
            for (unsigned side = 0; side < 2; ++side) gains[ch][side] += (target[ch][side] - gains[ch][side]) * smoothing;
        }
        if (out_left) out_left[n] += sample[0] * gains[0][0] + sample[1] * gains[1][0];
        if (out_right) out_right[n] += sample[0] * gains[0][1] + sample[1] * gains[1][1];
        if (!capture) continue;
        if(r.failure.load(std::memory_order_relaxed)||r.cancel.load(std::memory_order_relaxed)){capture=false;continue;}
        if (r.stop.load(std::memory_order_acquire)) { capture = false; continue; }
        if (state == REAR_WAITING) {
            float trigger = std::max((r.channels & 1) ? std::fabs(sample[0]) : 0.f,
                                     (r.channels & 2) ? std::fabs(sample[1]) : 0.f);
            if (r.threshold > 0 && trigger < r.threshold) continue;
            if (!r.state.compare_exchange_strong(state,REAR_RECORDING,std::memory_order_acq_rel)) {
                capture = false;
                continue;
            }
            state = REAR_RECORDING;
        }
        if (write - read >= ring_size) { r.failure.store(ERROR_OVERFLOW); capture = false; continue; }
        r.samples[write & (ring_size - 1)][0] = sample[0];
        r.samples[write & (ring_size - 1)][1] = sample[1];
        ++write;
        if (write >= r.limit) { r.stop.store(1); capture = false; }
    }
    if (state == REAR_WAITING || state == REAR_RECORDING) {
        r.frames.store(write, std::memory_order_relaxed);
        r.write.store(write, std::memory_order_release);
        if (r.stop.load()) {
            r.state.store(REAR_FINISHING,std::memory_order_release);
            r.stop.store(2,std::memory_order_release);
        }
    }
    for (unsigned ch = 0; ch < 2; ++ch) {
        float value = std::max(peak[ch], .85f * [&] { unsigned bits = meters[ch].load(); float f; std::memcpy(&f, &bits, 4); return f; }());
        unsigned bits; std::memcpy(&bits, &value, 4); meters[ch].store(bits);
    }
    audio_users.fetch_sub(1,std::memory_order_release);
}
extern "C" int rear_record_begin(const char *directory, unsigned channels, unsigned rate,
                                  float threshold, uint64_t limit) {
    auto &r = recording;
    if (r.state.load() != REAR_IDLE || r.worker.joinable() || !directory || !*directory ||
        !channels || channels > 3 || rate < 8000 || rate > 192000 || !std::isfinite(threshold)) return 0;
    r.channels = channels; r.rate = rate; r.threshold = std::max(0.f, threshold);
    constexpr uint64_t max_frames = (UINT32_MAX - 37) / 3;
    r.limit = limit ? std::min(limit,max_frames) : max_frames;
    if (std::snprintf(r.directory, sizeof(r.directory), "%s/Rear-XXXXXX", directory) >= (int)sizeof(r.directory)) return 0;
    if (!mkdtemp(r.directory)) return 0;
    for (unsigned ch = 0; ch < 2; ++ch) std::snprintf(r.paths[ch], sizeof(r.paths[ch]), "%.1023s/Rear-%c.wav", r.directory, 'L' + 6 * ch);
    r.write.store(0); r.read.store(0); r.frames.store(0); r.stop.store(0); r.cancel.store(0); r.failure.store(0);
    r.state.store(REAR_PREPARING);
    try { r.worker = std::thread(writer); }
    catch (...) { r.state.store(REAR_IDLE); rmdir(r.directory); return 0; }
    return 1;
}
extern "C" void rear_record_stop() {
    unsigned running = 0;
    recording.stop.compare_exchange_strong(running, 1, std::memory_order_acq_rel);
}
extern "C" void rear_record_cancel() { recording.cancel.store(1, std::memory_order_release); }
extern "C" unsigned rear_record_state() { return recording.state.load(std::memory_order_acquire); }
extern "C" uint64_t rear_record_frames() { return recording.frames.load(std::memory_order_relaxed); }
extern "C" const char *rear_record_error() {
    static const char *messages[] = {"", "Recording file error", "Recording buffer overflow", "Input sample rate changed", "No audio recorded", "Rear input changed"};
    unsigned error = recording.failure.load(); return error < sizeof(messages) / sizeof(*messages) ? messages[error] : "Recording failed";
}
extern "C" const char *rear_record_file(unsigned ch) { return ch < 2 ? recording.paths[ch] : ""; }
extern "C" void rear_record_release(int assigned) {
    auto &r = recording;
    unsigned state = r.state.load();
    if (state != REAR_IDLE && state != REAR_READY && state != REAR_ERROR) return;
    if (r.worker.joinable()) r.worker.join();
    if (!assigned && r.directory[0]) {
        for (unsigned ch = 0; ch < 2; ++ch) if (r.paths[ch][0]) unlink(r.paths[ch]);
        rmdir(r.directory);
    }
    r.directory[0] = r.paths[0][0] = r.paths[1][0] = 0;
    r.state.store(REAR_IDLE, std::memory_order_release);
}
