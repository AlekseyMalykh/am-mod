#include "rear_native.h"
#include "resample_queue.h"
#include "pad_resample_commit.h"
#include "pad_assignment.h"
#include "step_native.h"
extern "C" {
#include "workflow_ui.h"
}
#include <atomic>
#include <memory>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIELD(p,o,T) (*reinterpret_cast<T *>(reinterpret_cast<char *>(p)+(o)))
extern "C" void pad_source_layer_sample(void *,int,uintptr_t *,uintptr_t);
extern "C" void rear_load_sample(void *,const uint64_t *,uintptr_t *,uintptr_t,float);
namespace {
uintptr_t image;
struct Session;
std::atomic<Session *> session{nullptr};
std::atomic<unsigned> input_active{0}, monitor_active{0};
std::atomic<float> main_gain{0.f};
float current_main_gain = 0.f;
float raw[2][8192];
float monitoring[2][8192];
thread_local void *monitor_feedback_blocked = nullptr;

template<class T> T native(uintptr_t offset) { return reinterpret_cast<T>(image + offset); }
void monitor_feedback(void *properties, unsigned enabled) {
    // Native feedback mirrors the recorder's monitor switch to Off or On,
    // preserving Auto. The custom mix temporarily owns that switch.
    if (properties == monitor_feedback_blocked || FIELD(properties,0x98,int) == 1) return;
    native<void(*)(void *,unsigned)>(0x15636c0)(static_cast<char *>(properties)+0x48,(enabled & 0xffu) << 1);
}

bool install_monitor_feedback() {
    constexpr uint32_t expected[] = {
        0xd503245f,0xb9409802,0x12001c21,0x7100045f,0x54000080,0x531f7821,0x91012000,0x1400033d
    };
    auto *entry = reinterpret_cast<unsigned char *>(image + 0x15629b0);
    if (std::memcmp(entry,expected,sizeof(expected)) != 0) return false;
    auto *page = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(entry) & ~uintptr_t(4095));
    if (mprotect(page,4096,PROT_READ|PROT_WRITE) != 0) return false;
    // BTI followed by a jump through the address stored at entry + 16.
    constexpr uint32_t jump[] = {0xd503245f,0x58000070,0xd61f0200,0xd503201f};
    const auto callback = reinterpret_cast<uintptr_t>(monitor_feedback);
    std::memcpy(entry,jump,sizeof(jump));
    std::memcpy(entry+sizeof(jump),&callback,sizeof(callback));
    __builtin___clear_cache(reinterpret_cast<char *>(entry),reinterpret_cast<char *>(entry+24));
    if (mprotect(page,4096,PROT_READ|PROT_EXEC) != 0) _exit(121);
    return true;
}

void *lookup(void *services, uintptr_t type) {
    if (!services) return nullptr;
    void *lock = static_cast<char *>(services) + 0x38;
    if (native<int(*)(void *)>(0xb51828)(lock)) return nullptr;
    void *value = nullptr;
    try { auto **entry = native<void **(*)(void *,const uintptr_t *)>(0x143ad50)(services,&type); if (entry) value = *entry; }
    catch (...) {}
    native<void(*)(void *)>(0xb51808)(lock);
    return value;
}
struct Session {
    void *copy, *services, *properties_input = nullptr, *recorder = nullptr, *loader = nullptr;
    resample_queue *queue = nullptr;
    std::atomic<bool> live{true}, busy{false};
    std::atomic<uint64_t> epoch{1};
    std::atomic<unsigned> command{0}, status{0}, occupied{0};
    std::atomic<unsigned> phase{0}, stop_requested{0};
    std::atomic<int> source{-1}, monitor{0};
    bool monitor_suppressed = false;
    bool looked_up = false;
    unsigned overwrite = 0, mode = 0, targets[2]{};
    void *properties = nullptr, *program = nullptr, *pool = nullptr;
    uint64_t start_epoch = 0;
    uintptr_t before[2][8]{};
    std::mutex message_mutex;
    char message[128]{};
};
void message(Session *s, unsigned status, const char *text) {
    { std::lock_guard<std::mutex> lock(s->message_mutex); std::snprintf(s->message,sizeof(s->message),"%s",text); }
    s->status.store(status,std::memory_order_release);
}
bool model(Session *s) {
    if (!s->live.load(std::memory_order_acquire) || s->epoch.load() != s->start_epoch) return false;
    void *things = FIELD(s->copy,0x78,void *);
    return things && FIELD(s->copy,0xc8,void *) == s->properties &&
        FIELD(things,0xb8,void *) == s->program && FIELD(s->properties,0x1278,void *) == s->pool;
}
bool selected_channel(unsigned mode, unsigned channel) {
    return mode == REAR_DUAL || (mode == REAR_LEFT && !channel) || (mode == REAR_RIGHT && channel);
}
void *find_recorder(void *copy) {
    void *registry=FIELD(copy,0x98,void *);
    if(!registry)return nullptr;
    void *stack[32];unsigned count=0,seen=0;
    void *node=FIELD(registry,0x168,void *);
    while(node||count){
        while(node){if(count==32||++seen>256)return nullptr;stack[count++]=node;node=FIELD(node,0x10,void *);}
        node=stack[--count];void *mode=FIELD(node,0x28,void *);
        if(mode&&FIELD(mode,0,uintptr_t)==image+0x6d8df40)return FIELD(mode,0xe8,void *);
        node=FIELD(node,0x18,void *);
    }
    return nullptr;
}
bool same_targets(Session *s) {
    for (unsigned ch = 0; ch < 2; ++ch) {
        if (!selected_channel(s->mode,ch)) continue;
        void *instrument = native<void *(*)(void *,int)>(0x286bbe0)(s->program,(int)s->targets[ch]);
        if (!instrument || FIELD(instrument,0x2310,void *) != s->program) return false;
        for (unsigned layer = 0; layer < 8; ++layer) {
            uintptr_t sample[2]{}; pad_source_layer_sample(instrument,(int)layer,sample,image+0x2803b90);
            bool same = sample[0] == s->before[ch][layer]; pad_assignment_release(image,sample);
            if (!same) return false;
        }
    }
    return true;
}
struct Assignment {
    alignas(8) unsigned char header[0x50]{};
    uintptr_t table[11]{};
    Session *session;
    void *copy, *properties, *program, *pool;
    uint64_t epoch;
    pad_resample_commit *parts[2]{};
    unsigned count = 0;
    bool quarantined = false;
    explicit Assignment(Session *s): session(s),copy(s->copy),properties(s->properties),program(s->program),pool(s->pool),epoch(s->start_epoch) {
        uint64_t title = 0;
        native<void(*)(void *,const char *)>(0xf535c0)(&title,"Record Rear Inputs");
        try { native<void(*)(void *,const uint64_t *,int,int)>(0x19c9850)(this,&title,1,0); }
        catch (...) { native<void(*)(void *)>(0xf0a290)(&title); throw; }
        native<void(*)(void *)>(0xf0a290)(&title);
        std::memcpy(table,reinterpret_cast<void *>(image+0x6e439a0),sizeof(table));
    }
    ~Assignment() {
        for (unsigned n = 0; n < count; ++n) pad_commit_destroy(parts[n]);
        native<void(*)(void *)>(0xf0a290)(header+0x38);
        native<void(*)(void *)>(0xf0a290)(header+0x30);
        native<void(*)(void *)>(0x236f110)(header+8);
    }
};
int valid_assignment(void *p, pad_commit_operation) {
    auto *a = static_cast<Assignment *>(p);
    if (!a->session->live.load() || a->session->epoch.load() != a->epoch) return 0;
    void *things = FIELD(a->copy,0x78,void *);
    return things && FIELD(a->copy,0xc8,void *) == a->properties && FIELD(things,0xb8,void *) == a->program && FIELD(a->properties,0x1278,void *) == a->pool;
}
int undo(Assignment *a) noexcept {
    for (unsigned n = a->count; n > 0; --n) {
        auto *part = a->parts[n-1];
        if (pad_commit_undo(part)) continue;
        if (pad_commit_status(part) == PAD_COMMIT_RECOVERY_REQUIRED) a->quarantined = true;
        for (unsigned restore = n; restore < a->count; ++restore)
            if (!pad_commit_apply(a->parts[restore])) a->quarantined = true;
        return 1;
    }
    return 0;
}
int perform(Assignment *a) noexcept {
    for (unsigned n = 0; n < a->count; ++n) {
        if (pad_commit_apply(a->parts[n])) continue;
        if (pad_commit_status(a->parts[n]) == PAD_COMMIT_RECOVERY_REQUIRED) a->quarantined = true;
        while (n > 0) if (!pad_commit_undo(a->parts[--n])) a->quarantined = true;
        return 1;
    }
    return 0;
}
void dispose(Assignment *a) noexcept {
    if (a->quarantined) {
        message(a->session,3,"Assignment rollback needs recovery; recorded files retained");
        return;
    }
    delete a;
}
bool assign(Session *s) {
    if (!model(s) || !same_targets(s)) {
        std::fprintf(stderr,"REAR_ASSIGN stale project or destination\n");
        return false;
    }
    auto command = std::shared_ptr<Assignment>(new Assignment(s),dispose);
    command->table[0] = reinterpret_cast<uintptr_t>(dispose);
    command->table[1] = reinterpret_cast<uintptr_t>(dispose);
    command->table[8] = reinterpret_cast<uintptr_t>(perform);
    command->table[9] = reinterpret_cast<uintptr_t>(undo);
    FIELD(command.get(),0,uintptr_t *) = command->table;
    pad_commit_context context{image,s->properties,s->program,FIELD(s->copy,0x70,void *),s->pool,command.get(),valid_assignment,nullptr,nullptr};
    for (unsigned ch = 0; ch < 2; ++ch) {
        if (!selected_channel(s->mode,ch)) continue;
        uint64_t file = 0; uintptr_t sample[2]{};
        native<void(*)(void *,const char *)>(0xf535c0)(&file,rear_record_file(ch));
        try { rear_load_sample(s->loader,&file,sample,image+0x1df3db0,44100.f); }
        catch (...) { native<void(*)(void *)>(0xf0a290)(&file); pad_assignment_release(image,sample); std::fprintf(stderr,"REAR_ASSIGN loader threw\n"); return false; }
        native<void(*)(void *)>(0xf0a290)(&file);
        void *loaded = reinterpret_cast<void *>(sample[0]);
        bool metadata = loaded && FIELD(loaded,0x130,void *);
        int channels = metadata ? native<int(*)(void *)>(0x2941f70)(loaded) : 0;
        int frames = metadata ? native<int(*)(void *)>(0x2941f80)(loaded) : 0;
        bool correct = channels == 1 && frames > 0 && (uint64_t)frames == rear_record_frames();
        auto *part = correct ? pad_commit_prepare(&context,sample,s->targets[ch],(s->overwrite >> ch)&1) : nullptr;
        pad_assignment_release(image,sample);
        if (!part) {
            std::fprintf(stderr,"REAR_ASSIGN rejected channel=%u loaded=%d channels=%d frames=%d expected=%llu\n",
                         ch,loaded!=nullptr,channels,frames,(unsigned long long)rear_record_frames());
            return false;
        }
        command->parts[command->count++] = part;
    }
    int result = command->count ? native<int(*)(void *,const void *)>(0x19d0a00)(FIELD(s->copy,0x68,void *),&command) : -1;
    std::fprintf(stderr,"REAR_ASSIGN destinations=%u result=%d\n",command->count,result);
    return result == 0;
}
int owner_tick(void *p) {
    auto *s = static_cast<Session *>(p);
    unsigned command = s->command.exchange(0);
    if (!s->live.load()) return 0;
    if (command == 1) {
        if(s->stop_requested.load()) { s->busy.store(false); s->phase.store(0); return 0; }
        s->start_epoch = s->epoch.load();
        void *things = FIELD(s->copy,0x78,void *);
        s->properties = FIELD(s->copy,0xc8,void *);
        s->program = things ? FIELD(things,0xb8,void *) : nullptr;
        s->pool = s->properties ? FIELD(s->properties,0x1278,void *) : nullptr;
        s->mode = rear_setting(0,REAR_MODE);
        s->targets[0] = rear_setting(0,REAR_TARGET); s->targets[1] = rear_setting(1,REAR_TARGET);
        if (!s->program || !s->pool || !model(s) || !s->mode ||
            (s->mode == REAR_DUAL && s->targets[0] == s->targets[1])) throw "Choose two different destination pads";
        for (unsigned ch = 0; ch < 2; ++ch) {
            if (!selected_channel(s->mode,ch)) continue;
            void *instrument = native<void *(*)(void *,int)>(0x286bbe0)(s->program,(int)s->targets[ch]);
            if (!instrument || FIELD(instrument,0x2310,void *) != s->program) throw "Destination unavailable";
            bool used = false;
            for (unsigned layer = 0; layer < 8; ++layer) {
                uintptr_t sample[2]{}; pad_source_layer_sample(instrument,(int)layer,sample,image+0x2803b90);
                s->before[ch][layer] = sample[0]; used = used || sample[0]; pad_assignment_release(image,sample);
            }
            if (used && !(s->overwrite & (1u << ch))) throw "Confirm replacement of occupied pads";
        }
        void *properties = s->properties_input;
        if (!properties || s->source.load() != 1) throw "Select Rear as input source";
        /* Exact native FREE/SEQ length and threshold must be sampled before
         * arming. SEQ is enabled after transport-bound start is verified. */
        if (FIELD(properties,0xbb,unsigned char)&1) throw "Set Rec Length to Free for Dual Mono";
        float db = FIELD(properties,0x44,float);
        float threshold = db <= -96.f ? 0.f : std::pow(10.f,db/20.f);
        unsigned mask = s->mode == REAR_DUAL ? 3 : s->mode == REAR_LEFT ? 1 : 2;
        if (!rear_record_begin("/data/TempProjects/Temp-Project_[ProjectData]",mask,44100,threshold,0)) throw "Cannot create recording files";
        message(s,1,"Waiting for input / STOP to finish");
        s->phase.store(2,std::memory_order_release);
        if(s->stop_requested.load())rear_record_stop();
    } else if (command == 2) {
        bool applied = false;
        try { applied = assign(s); } catch (...) {}
        if (applied) { rear_record_release(1); message(s,2,"Recorded inputs assigned"); }
        else { message(s,3,"Cannot assign inputs; recording files retained"); }
        s->busy.store(false,std::memory_order_release);
        s->phase.store(0);
    }
    return 0;
}
int guarded_tick(void *p) {
    auto *s = static_cast<Session *>(p);
    try { return owner_tick(p); }
    catch (const char *error) { message(s,3,error); }
    catch (...) { message(s,3,"Rear recording setup failed"); }
    s->busy.store(false,std::memory_order_release); s->phase.store(0); return 0;
}
}
extern "C" void *rear_native_process(void *thread, void *position, void *buffer,rear_render_fn original) {
    int channels = FIELD(buffer,0,int), frames = FIELD(buffer,4,int);
    auto **pcm = FIELD(buffer,0x10,float **);
    bool capture = input_active.load(std::memory_order_acquire) && channels >= 2 && frames > 0 && frames <= 8192 && pcm && pcm[0] && pcm[1];
    if (capture) {
        if (FIELD(buffer,0x128,unsigned char)&1) std::memset(raw,0,sizeof(raw));
        else { std::memcpy(raw[0],pcm[0],(size_t)frames*sizeof(float)); std::memcpy(raw[1],pcm[1],(size_t)frames*sizeof(float)); }
    }
    void *result=original(thread,position,buffer);
    if (!capture) return result;
    pcm = FIELD(buffer,0x10,float **);
    bool monitor = monitor_active.load(std::memory_order_acquire) != 0;
    std::memset(monitoring[0],0,(size_t)frames*sizeof(float));
    std::memset(monitoring[1],0,(size_t)frames*sizeof(float));
    rear_audio_process(raw[0],raw[1],monitoring[0],monitoring[1],(unsigned)frames,44100,monitor);
    if (monitor && pcm && FIELD(buffer,0,int)>=2) {
        float target_gain = main_gain.load(std::memory_order_relaxed);
        for (int n = 0; n < frames; ++n) {
            current_main_gain += (target_gain - current_main_gain) * .004525f;
            for (int ch = 0; ch < FIELD(buffer,0,int); ++ch)
                if (pcm[ch]) pcm[ch][n] += monitoring[ch&1][n] * (ch < 2 ? current_main_gain : 1.f);
        }
        FIELD(buffer,0x128,unsigned char) = 0;
    }
    return result;
}

extern "C" int rear_native_install(uintptr_t base) {
    image = base;
    auto *instruction = reinterpret_cast<uint32_t *>(base+0x14e9cfc);
    constexpr uint32_t expected = 0x94000000u | ((0x18d7630u-0x14e9cfcu)/4);
    if (*instruction != expected || sysconf(_SC_PAGESIZE) != 4096) return 0;
    return install_monitor_feedback();
}
extern "C" void rear_native_bind(void *copy, void *services) {
    auto *s = new(std::nothrow)Session{}; if (!s) return;
    s->copy = copy; s->services = services;
    s->queue = resample_queue_create(image,copy,guarded_tick,s);
    session.store(s,std::memory_order_release);
}
extern "C" void rear_native_dead(void *copy) {
    auto *s = session.load(); if (!s || s->copy != copy) return;
    s->live.store(false); s->epoch.fetch_add(1); resample_queue_stop(s->queue);
    rear_record_cancel(); input_active.store(0); monitor_active.store(0);
}
extern "C" void rear_native_project_changed() {
    auto *s = session.load(); if (s) s->epoch.fetch_add(1);
    rear_record_cancel();
}
extern "C" void rear_native_tick(int visible) {
    auto *s = session.load(); if (!s || !s->live.load()) return;
    if(!s->looked_up){
        s->looked_up=true;
        auto *engine=lookup(s->services,image+0x6e0c988);
        s->properties_input=engine&&FIELD(engine,0,uintptr_t)==image+0x6e0cf38?FIELD(engine,0x5e0,void *):nullptr;
        s->recorder=find_recorder(s->copy);
        if(!s->properties_input&&s->recorder&&FIELD(s->recorder,0,uintptr_t)==image+0x6d8de80)s->properties_input=FIELD(s->recorder,0xc0,void *);
        s->loader=lookup(s->services,image+0x6e08dd0);
        std::fprintf(stderr,"REAR_BIND properties=%u recorder=%u loader=%u queue=%u\n",s->properties_input!=nullptr,s->recorder!=nullptr,s->loader!=nullptr,s->queue!=nullptr);
    }
    if(!s->recorder&&visible)s->recorder=find_recorder(s->copy);
    void *properties = s->properties_input;
    if (!properties) return;
    int source = FIELD(properties,0x24,int), monitor = FIELD(properties,0x98,int);
    s->source.store(source); s->monitor.store(monitor);
    bool rear = source == 1;
    bool enabled = rear_setting(0,REAR_ENABLED) != 0;
    bool available = s->recorder && FIELD(s->recorder,0,uintptr_t) == image+0x6d8de80;
    void *recorder=available?FIELD(s->recorder,0x88,void *):nullptr;
    bool have_monitor=recorder&&FIELD(recorder,0,uintptr_t)==image+0x6da3008;
    if (have_monitor) {
        bool suppress = rear && enabled;
        if (suppress || s->monitor_suppressed) {
            void *previous = monitor_feedback_blocked;
            monitor_feedback_blocked = properties;
            native<void(*)(void *,int)>(0x13904f0)(static_cast<char *>(recorder)+0x190,
                !suppress && (monitor==2 || (monitor==1 && visible)));
            monitor_feedback_blocked = previous;
        }
        s->monitor_suppressed = suppress;
    }
    input_active.store(rear,std::memory_order_release);
    bool custom_monitor = rear && have_monitor && enabled && (monitor==2||(monitor==1&&visible));
    if (custom_monitor) main_gain.store(std::fmax(0.f, workflow_main_output_gain()), std::memory_order_relaxed);
    monitor_active.store(custom_monitor,std::memory_order_release);
    unsigned state = rear_record_state();
    if (s->phase.load(std::memory_order_acquire)==2) {
        if ((source != 1 || !model(s)) && state >= REAR_PREPARING && state <= REAR_FINISHING) rear_record_cancel();
        if (state == REAR_READY) {
            s->phase.store(3);s->command.store(2);
            if (!resample_queue_wake(s->queue)) { s->command.store(0);s->phase.store(2); }
        } else if (state == REAR_ERROR) { message(s,3,rear_record_error()); s->busy.store(false);s->phase.store(0); }
        else if (state == REAR_IDLE) { rear_record_release(0); message(s,3,"Recording cancelled"); s->busy.store(false);s->phase.store(0); }
    }
    unsigned occupied = 0;
    void *things = FIELD(s->copy,0x78,void *); void *program = things ? FIELD(things,0xb8,void *) : nullptr;
    if (program) for (unsigned ch = 0; ch < 2; ++ch) {
        void *instrument = native<void *(*)(void *,int)>(0x286bbe0)(program,(int)rear_setting(ch,REAR_TARGET));
        if (instrument && native<int(*)(void *)>(0x2804760)(instrument)) occupied |= 1u << ch;
    }
    s->occupied.store(occupied);
}
extern "C" int rear_native_start(unsigned overwrite) {
    auto *s = session.load();
    if (!s || !s->live.load() || !s->loader || !s->properties_input || !s->queue || s->busy.exchange(true)) return 0;
    if (rear_record_state() != REAR_IDLE) { s->busy.store(false); return 0; }
    s->overwrite = overwrite; s->stop_requested.store(0);s->phase.store(1);s->command.store(1);
    if (resample_queue_wake(s->queue)) return 1;
    s->command.store(0); s->phase.store(0);s->busy.store(false); return 0;
}
extern "C" int rear_native_busy() { auto *s = session.load(); return s && s->busy.load(); }
extern "C" int rear_native_source() { auto *s = session.load(); return s ? s->source.load() : -1; }
extern "C" int rear_native_monitor() { auto *s = session.load(); return s ? s->monitor.load() : 0; }
extern "C" void rear_native_monitor_on() {
    auto *s=session.load();
    if(s&&s->live.load()&&s->properties_input)
        native<void(*)(void *,unsigned)>(0x15636c0)(static_cast<char *>(s->properties_input)+0x48,2);
}
extern "C" void rear_native_stop() {
    auto *s=session.load();if(s)s->stop_requested.store(1);
    rear_record_stop();
}
extern "C" unsigned rear_native_occupied() { auto *s = session.load(); return s ? s->occupied.load() : 0; }
extern "C" unsigned rear_native_bank_occupied(unsigned bank) {
    auto *s = session.load();
    if (!s || !s->live.load() || bank >= 8) return 0;
    void *things = FIELD(s->copy,0x78,void *);
    void *program = things ? FIELD(things,0xb8,void *) : nullptr;
    if (!program) return 0;
    unsigned occupied = 0;
    for (unsigned pad = 0; pad < 16; ++pad) {
        void *instrument = native<void *(*)(void *,int)>(0x286bbe0)(program,(int)(bank*16+pad));
        if (instrument && native<int(*)(void *)>(0x2804760)(instrument)) occupied |= 1u << pad;
    }
    return occupied;
}
extern "C" unsigned rear_native_status() { auto *s = session.load(); return s ? s->status.load() : 0; }
extern "C" const char *rear_native_message() {
    static thread_local char text[128]; auto *s = session.load(); if (!s) return "Rear input services unavailable";
    std::lock_guard<std::mutex> lock(s->message_mutex); std::memcpy(text,s->message,sizeof(text)); return text;
}
extern "C" void rear_native_clear() { auto *s = session.load(); if (!s || s->busy.load()) return; rear_record_release(1); message(s,0,""); }
