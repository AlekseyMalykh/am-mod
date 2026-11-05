#include "src/rear_audio.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>

static void wait_state(unsigned want) {
    for (unsigned n = 0; n < 1000 && rear_record_state() != want; ++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(rear_record_state() == want);
}
static void read_file(const char *path, float expected, unsigned frames) {
    FILE *f = std::fopen(path, "rb"); assert(f);
    unsigned char h[44]; assert(std::fread(h, 1, 44, f) == 44);
    assert(!std::memcmp(h, "RIFF", 4) && !std::memcmp(h + 8, "WAVEfmt ", 8));
    assert(h[20] == 1 && h[22] == 1 && h[34] == 24);
    unsigned bytes = h[40] | unsigned(h[41]) << 8 | unsigned(h[42]) << 16 | unsigned(h[43]) << 24;
    assert(bytes == frames * 3);
    for (unsigned n = 0; n < frames; ++n) {
        unsigned char b[3]; assert(std::fread(b, 1, 3, f) == 3);
        int pcm = int(b[0] | unsigned(b[1]) << 8 | unsigned(b[2]) << 16) << 8;
        assert(std::fabs(float(pcm / 256) / 8388607.f - expected) < 2e-7f);
    }
    if (bytes & 1) assert(std::fgetc(f) == 0);
    assert(std::fgetc(f) == EOF); std::fclose(f);
}
int main() {
    rear_settings_reset();
    assert(rear_setting(0, REAR_LEVEL) == 100 && rear_setting(1, REAR_LEVEL) == 100);
    assert(rear_setting(0, REAR_PAN) == 0 && rear_setting(1, REAR_PAN) == 100);
    assert(rear_setting(0, REAR_TARGET) == 0 && rear_setting(1, REAR_TARGET) == 1);
    uint64_t defaults = rear_settings_snapshot();
    assert(rear_settings_valid(defaults) && !rear_settings_valid(UINT64_MAX));
    assert(!rear_set_setting(0, REAR_LEVEL, 101));
    assert(rear_set_setting(0, REAR_ENABLED, 1));
    float left[1024], right[1024], out_left[1024]{}, out_right[1024]{};
    for (unsigned n = 0; n < 1024; ++n) { left[n] = .25f; right[n] = -.125f; }
    for (unsigned n = 0; n < 20; ++n) {
        std::memset(out_left, 0, sizeof(out_left)); std::memset(out_right, 0, sizeof(out_right));
        rear_audio_process(left, right, out_left, out_right, 1024, 44100, 1);
    }
    assert(std::fabs(out_left[1023] - .25f) < 1e-5f && std::fabs(out_right[1023] + .125f) < 1e-5f);
    assert(rear_set_setting(0, REAR_MONITOR, 0));
    for (unsigned n = 0; n < 20; ++n) {
        std::memset(out_left, 0, sizeof(out_left)); std::memset(out_right, 0, sizeof(out_right));
        rear_audio_process(left, right, out_left, out_right, 1024, 44100, 1);
    }
    assert(std::fabs(out_left[1023]) < 1e-5f && std::fabs(out_right[1023] + .125f) < 1e-5f);
    assert(rear_set_setting(1, REAR_PAN, 50));
    assert(rear_set_setting(1, REAR_LEVEL, 50));
    for (unsigned n = 0; n < 20; ++n) {
        std::memset(out_left, 0, sizeof(out_left)); std::memset(out_right, 0, sizeof(out_right));
        rear_audio_process(left, right, out_left, out_right, 1024, 44100, 1);
    }
    assert(std::fabs(out_left[1023] + .125f * .5f * std::sqrt(.5f)) < 1e-5f);
    assert(std::fabs(out_left[1023] - out_right[1023]) < 1e-5f);
    char directory[] = "/tmp/am-rear-test-XXXXXX"; assert(mkdtemp(directory));
    assert(rear_record_begin(directory, 3, 44100, 0, 2048));
    assert(!rear_record_begin(directory, 3, 44100, 0, 2048));
    wait_state(REAR_WAITING);
    assert(!rear_set_setting(0, REAR_TARGET, 4));
    rear_audio_process(left, right, nullptr, nullptr, 1024, 44100, 0);
    rear_audio_process(left, right, nullptr, nullptr, 1024, 44100, 0);
    wait_state(REAR_READY);
    assert(rear_record_frames() == 2048);
    read_file(rear_record_file(0), .25f, 2048);
    read_file(rear_record_file(1), -.125f, 2048);
    rear_record_release(0);
    assert(rear_record_state() == REAR_IDLE);
    assert(rear_record_begin(directory, 1, 44100, 0, 0));
    wait_state(REAR_WAITING);
    rear_audio_process(left, right, nullptr, nullptr, 101, 44100, 0);
    rear_record_stop();
    rear_audio_process(left, right, nullptr, nullptr, 101, 44100, 0);
    rear_record_stop();
    wait_state(REAR_READY);
    read_file(rear_record_file(0), .25f, 101);
    rear_record_release(0);
    assert(rear_record_begin(directory, 1, 44100, .5f, 2048));
    wait_state(REAR_WAITING);
    rear_audio_process(left, right, nullptr, nullptr, 1024, 44100, 0);
    assert(rear_record_state() == REAR_WAITING && rear_record_frames() == 0);
    rear_record_cancel(); wait_state(REAR_IDLE); rear_record_release(0);
    assert(rear_record_begin(directory, 2, 44100, 0, 2048));
    wait_state(REAR_WAITING);
    rear_audio_process(left, right, nullptr, nullptr, 1024, 48000, 0);
    wait_state(REAR_ERROR);
    assert(!std::strcmp(rear_record_error(), "Input sample rate changed"));
    rear_record_release(0); assert(!rmdir(directory));
    rear_settings_restore(defaults); assert(rear_settings_snapshot() == defaults);
    std::puts("PASS independent monitor, level and equal-power pan; smoothing; dry simultaneous PCM24 mono files, common exact length, threshold, cancellation, rate mismatch and cleanup");
}
