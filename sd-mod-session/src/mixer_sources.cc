#include "mixer_sources.h"
#include "mixer_archive.h"
#include "resample_queue.h"
#include <atomic>
#include <cstring>
#include <ctime>
#include <mutex>
#include <new>

extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);

namespace {
template<class T> T field(void *object,unsigned offset)
{
    T value;
    std::memcpy(&value,static_cast<char*>(object)+offset,sizeof(value));
    return value;
}
struct Source {
    uintptr_t image;
    void *context;
    resample_queue *queue = nullptr;
    std::atomic<bool> live{true};
    std::mutex mutex;
    mixer_sources_snapshot request{},snapshot{};
    Source *next = nullptr;
};
std::mutex sources_mutex;
Source *current = nullptr;
Source *retired = nullptr;
uint64_t serial = 0;

bool context_valid(Source *source)
{
    uintptr_t type = field<uintptr_t>(source->context,0);
    return type == source->image+0x6d8cd88 || type == source->image+0x6e0dfd8;
}

int capture(void *user)
{
    auto *source = static_cast<Source*>(user);
    if (!source->live.load(std::memory_order_acquire)) return 0;
    mixer_sources_snapshot result;
    {
        std::lock_guard<std::mutex> lock(source->mutex);
        result = source->request;
        source->snapshot = {};
    }
    if (mixer_archive_loading() || result.archive_generation != mixer_archive_generation() ||
        !context_valid(source)) return 0;
    void *things = field<void*>(source->context,0x78);
    if (!things || field<void*>(things,0xb8) != result.program) return 0;
    result.present[0] = result.present[1] = 0;
    for (unsigned pad = 0; pad < 128; ++pad) {
        void *instrument = reinterpret_cast<void*(*)(void*,int)>(source->image+0x286bbe0)
            (result.program,static_cast<int>(pad));
        if (!instrument) continue;
        if (field<void*>(instrument,0x2310) != result.program) return 0;
        /*2803b90 requires eight actual layers. Validate its array before calls. */
        uintptr_t first = field<uintptr_t>(instrument,0x22e8);
        uintptr_t end = field<uintptr_t>(instrument,0x22f0);
        if (!first || end < first || end-first != 8*0xb30) return 0;
        for (unsigned layer = 0; layer < 8; ++layer) {
            uintptr_t sample[2]{};
            pad_source_layer_sample(instrument,static_cast<int>(layer),sample,source->image+0x2803b90);
            bool occupied = sample[0] != 0;
            if (sample[1]) reinterpret_cast<void(*)(void*)>(source->image+0x123bca0)
                (reinterpret_cast<void*>(sample[1]));
            if (occupied) {
                result.present[pad/64] |= UINT64_C(1) << (pad%64);
                break;
            }
        }
    }
    if (!source->live.load(std::memory_order_acquire) || mixer_archive_loading() ||
        result.archive_generation != mixer_archive_generation() ||
        field<void*>(things,0xb8) != result.program) return 0;
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC,&now)) return 0;
    result.captured_ms = static_cast<uint64_t>(now.tv_sec)*1000+now.tv_nsec/1000000;
    {
        std::lock_guard<std::mutex> lock(source->mutex);
        source->snapshot = result;
    }
    return 0;
}

void stop(Source *source)
{
    source->live.store(false,std::memory_order_release);
    resample_queue_stop(source->queue);
    source->next = retired;
    retired = source;
}

void collect()
{
    Source **link = &retired;
    while (*link) {
        Source *source = *link;
        if (resample_queue_destroy(source->queue)) {
            *link = source->next;
            delete source;
        } else link = &source->next;
    }
}
}

extern "C" void mixer_sources_bind(uintptr_t image,void *context)
{
    if (!image || !context) return;
    auto *source = new(std::nothrow)Source;
    if (!source) return;
    source->image = image;
    source->context = context;
    if (!context_valid(source)) { delete source; return; }
    source->queue = resample_queue_create(image,context,capture,source);
    if (!source->queue) { delete source; return; }
    std::lock_guard<std::mutex> lock(sources_mutex);
    if (current) stop(current);
    current = source;
    collect();
}

extern "C" void mixer_sources_stop(void *context)
{
    std::lock_guard<std::mutex> lock(sources_mutex);
    if (current && current->context == context) {
        stop(current);
        current = nullptr;
    }
}

extern "C" int mixer_sources_poll(void *program,uint64_t epoch,mixer_sources_snapshot *out)
{
    if (!out) return 0;
    *out = {};
    std::lock_guard<std::mutex> lock(sources_mutex);
    collect();
    if (!current || !program || !epoch || mixer_archive_loading()) return 0;
    uint64_t generation = mixer_archive_generation();
    {
        std::lock_guard<std::mutex> guard(current->mutex);
        *out = current->snapshot;
        current->request = {};
        current->request.program = program;
        current->request.source_epoch = epoch;
        current->request.archive_generation = generation;
        current->request.serial = ++serial;
    }
    resample_queue_wake(current->queue);
    return out->serial && out->program == program && out->source_epoch == epoch &&
        out->archive_generation == generation;
}
