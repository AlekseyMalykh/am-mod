#include "resample_fast_native.h"
#include "resample_fast_plan.h"
#include "resample_fault.h"
#include "resample_file_probe.h"
#include "resample_output_file.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <unistd.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);
namespace {
std::atomic<uintptr_t> image{0};
std::mutex install_lock;
std::mutex job_lock;
std::string job_path;
ResampleOutputFile job_output;
void*job_services=nullptr;
unsigned job_source=128;
bool armed=false;
std::atomic<unsigned> failure{0};
std::atomic<unsigned> job_state{0};
std::atomic<bool> installed{false};
using Input=void(*)(void*,int64_t,uintptr_t,void*,const void*);
using Dispatch=void(*)(void*,int64_t,void*,const void*,unsigned,unsigned);
Input input_original;
Dispatch dispatch_original;
struct Mini {
    void*context,*engine,*track;
    void*physical_output=nullptr;
    alignas(8) unsigned char note[56];
    uint64_t floor=0,minimum=0,budget=0;
    unsigned rate=0,started=0,failed=0;
    unsigned blocks=0,direct_blocks=0,tap_blocks=0,primary_blocks=0,dispatches=0,blocked=0;
    uint64_t scanned_block=UINT64_MAX;
    float peak=0;
    bool delegated=false,closed=false,tap_to_primary=false;
    resample_fast_tail tail{};
};
thread_local Mini*mini;
struct WriterTrace {void*context=nullptr;unsigned blocks=0,logs=0;bool audible=false;};
thread_local WriterTrace writer_trace;
float buffer_peak(void*a){
 if(!a||F(a,0,int)<2||F(a,4,int)<0||F(a,4,int)>8192||!F(a,16,float**)||(F(a,0x128,unsigned char)&1))return 0;
 float peak=0;auto**ch=F(a,16,float**);if(!ch[0]||!ch[1])return 0;
 for(int n=0;n<F(a,4,int);++n)for(unsigned k=0;k<2;++k){float v=std::fabs(ch[k][n]);if(std::isfinite(v)&&v>peak)peak=v;}
 return peak;
}
void trace_writer(void*w,const char*stage,float bus_peak){
 auto&t=writer_trace;if(!t.context||!w||t.logs>=6)return;
 uintptr_t begin=F(w,0x30,uintptr_t),end=F(w,0x38,uintptr_t);void*a=begin&&end>=begin&&end-begin>=0x130?reinterpret_cast<void*>(begin):nullptr;
 float peak=buffer_peak(a);bool first_audio=bus_peak>0&&!t.audible;
 if(t.blocks>2&&!first_audio)return;
 if(first_audio)t.audible=true;
 ++t.logs;
 uintptr_t first=F(w,0x18,uintptr_t),last=F(w,0x20,uintptr_t);void*stream=first&&last>first?F(reinterpret_cast<void*>(first),0,void*):nullptr;
 const char*path=F(w,0xd8,const char*);
 std::fprintf(stderr,"RESAMPLE WRITER_TRACE source=%u block=%u stage=%s bus_peak=%.9g primary_peak=%.9g clear=%u channels=%d frames=%d files=%llu stream_vtable=%llx format=%u depth=%u rate=%u flags=%u normalize=%.9g position=%llu fade_start=%llu fade_end=%llu path=%.240s\n",
 F(t.context,0x3f0,unsigned),t.blocks,stage,(double)bus_peak,(double)peak,a?F(a,0x128,unsigned char):255,a?F(a,0,int):0,a?F(a,4,int):0,
 (unsigned long long)(first&&last>=first?(last-first)/8:0),(unsigned long long)(stream?F(stream,0,uintptr_t)-image.load():0),F(w,0,unsigned),F(w,8,unsigned),F(w,12,unsigned),F(w,20,unsigned char),(double)F(w,0x100,float),
 (unsigned long long)F(w,0xe0,uint64_t),(unsigned long long)F(w,0xe8,uint64_t),(unsigned long long)F(w,0xf0,uint64_t),path?path:"(null)");
}

void input(void*engine,int64_t time,uintptr_t position,void*track,const void*event){
    (void)image.load(std::memory_order_acquire);
    if(mini&&engine==mini->engine&&event!=mini->note){++mini->blocked;return;}
    input_original(engine,time,position,track,event);
}
void dispatch(void*engine,int64_t time,void*track,const void*event,unsigned source,unsigned mask){
    (void)image.load(std::memory_order_acquire);
    if(mini&&engine==mini->engine){
        if(event!=mini->note){
            if(++mini->blocked<=3)std::fprintf(stderr,"RESAMPLE MINI_DISPATCH_BLOCKED source=%u same_track=%u type=%u\n",source,track==mini->track,event?static_cast<const unsigned char*>(event)[8]:255);
            return;
        }
        ++mini->dispatches;
    }
    dispatch_original(engine,time,track,event,source,mask);
}
void cancel(Mini&m,unsigned reason){
    m.failed=reason;failure.store(reason,std::memory_order_release);
    std::fprintf(stderr,"RESAMPLE MINI_REJECT reason=%u delegated=%u\n",reason,m.delegated);
    /* The same native cancel flag inspected by18b97c0. Do not continue a
     * nondecaying loop forever and do not label a budget cutoff as success. */
    __atomic_store_n(reinterpret_cast<unsigned char*>(m.context)+0xa18,1,__ATOMIC_RELEASE);
    /* The replaced native renderer normally closes this writer. A reject
     * before handing off to Seq must do that too; no import/assignment. */
    if(!m.delegated&&!m.closed){m.closed=true;void*w=F(m.context,0xa38,void*);if(w)reinterpret_cast<void(*)(void*)>(image+0x18b4500)(w);}
}
void prepare(void*context,void*engine){
    reinterpret_cast<void(*)(void*,void*)>(image+0x18b5bc0)(context,engine);
    if(!mini||mini->context!=context||mini->engine!=engine)return;
    auto&m=*mini;
    /* Source duration is supplied independently of the user's sequence. */
    if(!m.minimum||m.minimum>(UINT64_MAX-(uint64_t)m.rate*31)/16){cancel(m,1);return;}
    /* Allow slower pitch/warp to outlast the raw-file floor. Blocks stream
     * through the stock writer; this is a ceiling, not an allocation size.
     * Exhaustion is failure, never a successful truncated sample. */
    m.budget=m.minimum*16+(uint64_t)m.rate*31;
    if(m.budget>(UINT32_MAX-4096)/6){cancel(m,1);return;}
    if(!resample_fast_tail_init(&m.tail,m.minimum,m.rate/2,m.budget,0.00001f)){cancel(m,1);return;}
    F(context,0x9f0,uint64_t)=0;
    F(context,0x9f8,uint64_t)=m.budget;F(context,0xa00,uint64_t)=m.budget;
}
void frame(void*engine,void*clock){
    /* This wrapper is installed only at the common offline loop callsite:
     * x1 is exactly render-context+8 for both native Seq and private Sample. */
    void*context=static_cast<char*>(clock)-8;
    if(writer_trace.context!=context||F(context,0x9f0,uint64_t)==0){writer_trace=WriterTrace{};writer_trace.context=context;}
    ++writer_trace.blocks;
    reinterpret_cast<void(*)(void*,void*)>(image+0x2114c00)(engine,clock);
    if(!mini||mini->engine!=engine||mini->failed)return;
    auto&m=*mini;
    ++m.blocks;
    if(!m.started){
        m.started=1;
        resample_fault_stage(8);
        /* Exact native sequence boundary: source2 / mask7ffffffe. Its
         * normalizer and track processing remain native. Only this private
         * event passes the two thread-local sequence gates. */
        input(engine,0,0,m.track,m.note);
        resample_fault_stage(9);
        std::fprintf(stderr,"RESAMPLE MINI_NOTE_ON minimum=%llu rate=%u\n",(unsigned long long)m.minimum,m.rate);
    }
}
bool uses_tap(const Mini&m){return F(m.context,0x3f0,unsigned)!=0&&(F(m.context,0x4a0,unsigned char)&1);}
unsigned scan_block(void*audio,unsigned frames){
    unsigned count=frames;
    if(mini&&!mini->failed){
        auto&m=*mini;
        uint64_t block=F(m.context,0x9f0,uint64_t);
        if(m.scanned_block==block)return count;
        m.scanned_block=block;
        if(!m.tail.scanned)resample_fault_stage(10);
        if(!audio||frames>8192||F(audio,0,int)<2||F(audio,4,int)<0||frames>(unsigned)F(audio,4,int)||!F(audio,16,float**))cancel(m,2);
        else{
            auto**channels=F(audio,16,float**);uint64_t before=m.tail.scanned;
            /* Native AudioBuffer::isClear can leave old floats in its backing
             * store. The stock writer treats them as zero: use the same view. */
            static const float zero[8192]={};bool clear=F(audio,0x128,unsigned char)!=0;
            if(!clear)for(unsigned n=0;n<frames;++n)for(unsigned c=0;c<2;++c){float v=std::fabs(channels[c][n]);if(v>m.peak)m.peak=v;}
            unsigned state=resample_fast_tail_scan(&m.tail,clear?zero:channels[0],clear?zero:channels[1],frames);
            if(state==RFT_QUIET){
                count=(unsigned)(m.tail.scanned-before);
                /* Caller subtracts its original block count after write.
                 * Set remaining to that count so it reaches exactly zero. */
                F(m.context,0x9f8,uint64_t)=frames;
                std::fprintf(stderr,"RESAMPLE MINI_QUIET frames=%llu\n",(unsigned long long)m.tail.scanned);
            }else if(state!=RFT_MORE)cancel(m,state==RFT_NEEDS_MORE_ROOM?3:2);
        }
    }
    return count;
}
void write_block(void*writer,void*audio,void*gain,unsigned frames){
    if(mini)++mini->direct_blocks;
    unsigned count=mini&&uses_tap(*mini)?frames:scan_block(audio,frames);
    if(!mini||!mini->failed){
        reinterpret_cast<void(*)(void*,void*,void*,unsigned)>(image+0x18b8dd0)(writer,audio,gain,count);
        trace_writer(writer,"direct",buffer_peak(audio));
    }
}
void tap_block(void*writer,void*audio,void*key,float gain){
    unsigned frames=0;
    if(mini){
        ++mini->tap_blocks;
        /* The native "include master" tap is AudioRoute destination1,
         * subIndex0. Pads can also route directly to destination2 (physical
         * outputs), bypassing that intermediate bus. Native2226fb0 has now
         * finished the downstream mix: capture output pair0, including both
         * paths, rather than summing them and doubling master-routed pads. */
        if(mini->tap_to_primary&&mini->physical_output)audio=mini->physical_output;
        int capacity=audio?F(audio,4,int):0;
        uint64_t remaining=F(mini->context,0x9f8,uint64_t);
        frames=capacity>0?(unsigned)(remaining<(unsigned)capacity?remaining:(unsigned)capacity):0;
        if(!frames)cancel(*mini,2);else scan_block(audio,frames);
    }
    if(mini&&mini->failed)return;
    if(mini&&mini->tap_to_primary){
        /*18b94f0 registers a separate master STEM, not the primary file
         * imported by Sample. With output+8 null the primary accumulator
         * never receives audio. Feed this already mixed stereo bus through
         * the stock primary accumulator instead. Unity is required here:
         * the unresolved direct-route gain can be zero; pad/FX gain is
         * already present in this buffer.18b3b10 still flushes the file. */
        float level[3]={1.f,1.f,0.f};
        reinterpret_cast<void(*)(void*,void*,void*,unsigned)>(image+0x18b8dd0)(writer,audio,level,frames);
        ++mini->primary_blocks;
        trace_writer(writer,"master-to-primary",buffer_peak(audio));
    }else{
        reinterpret_cast<void(*)(void*,void*,void*,float)>(image+0x18b94f0)(writer,audio,key,gain);
        trace_writer(writer,"master-stem",buffer_peak(audio));
    }
}

uint64_t source_floor(void*program,unsigned pad,unsigned rate){
    void*instrument=reinterpret_cast<void*(*)(void*,int)>(image+0x286bbe0)(program,(int)pad);
    if(!instrument||F(instrument,0x2310,void*)!=program)return 0;
    uintptr_t layers=F(instrument,0x22e8,uintptr_t),end=F(instrument,0x22f0,uintptr_t);
    if(!layers||end<layers||end-layers!=8*0xb30)return 0;
    uint64_t maximum=0;
    for(int layer=0;layer<8;++layer){uintptr_t sample[2]{};pad_source_layer_sample(instrument,layer,sample,image+0x2803b90);
        if(sample[0]){
            void*p=reinterpret_cast<void*>(sample[0]);int n=reinterpret_cast<int(*)(void*)>(image+0x2941f80)(p),hz=reinterpret_cast<int(*)(void*)>(image+0x2941f90)(p);
            if(n>0&&hz>=8000&&hz<=192000){uint64_t frames=((uint64_t)n*rate+hz-1)/hz;if(frames>maximum)maximum=frames;}
        }
        if(sample[1])reinterpret_cast<void(*)(void*)>(image+0x123bca0)(reinterpret_cast<void*>(sample[1]));
    }return maximum;
}
/* DrumProgram::noteOn (286fcf0/286f890) launches the source and its four
 * DIRECT SimultPlay targets. It does not recurse into the targets' links.
 * Mirror that set only for duration/Loop handling; native DSP still launches
 * the voices, including duplicate targets, mute targets and pad processing. */
struct PadGroup {
    void*instrument[5]{};unsigned pad[5]{},count=0;
    bool add(void*program,unsigned n){
        for(unsigned k=0;k<count;++k)if(pad[k]==n)return true;
        if(n>=128||count>=5)return false;
        void*i=reinterpret_cast<void*(*)(void*,int)>(image+0x286bbe0)(program,(int)n);
        if(!i||F(i,0x2310,void*)!=program)return false;
        instrument[count]=i;pad[count++]=n;return true;
    }
    bool begin(void*program,unsigned source){
        if(!add(program,source))return false;
        if(!(F(instrument[0],0x234d,unsigned char)&1))return true;
        for(unsigned k=0;k<4;++k){
            /* Actual Instrument getter: kSimultPlayPad1..4 return the native
             * one-based pad number divided by128 (zero means no link). */
            float v=reinterpret_cast<float(*)(void*,unsigned)>(image+0x2806330)(instrument[0],0x404+k)*128.f;
            if(!std::isfinite(v)||v<0||v>128||std::floor(v)!=v)return false;
            if(!v)continue;
            unsigned n=(unsigned)v-1;
            void*i=reinterpret_cast<void*(*)(void*,int)>(image+0x286bbe0)(program,(int)n);
            if(!i)continue; // Native noteOn also skips missing/muted targets.
            if(F(i,0x2310,void*)!=program)return false;
            if(!(F(i,0x2e0,unsigned char)&1)&&!add(program,n))return false;
        }
        return true;
    }
};
struct OnePass {
    void*property[40]{};unsigned saved[40]{},count=0,changed=0;
    bool applied=false;
    bool begin(const PadGroup&group){
        /* Validate every layer before changing any setting. */
        for(unsigned k=0;k<group.count;++k){
            void*i=group.instrument[k];uintptr_t first=F(i,0x22e8,uintptr_t),end=F(i,0x22f0,uintptr_t);
            if(!first||end<first||end-first!=8*0xb30)return false;
            for(unsigned n=0;n<8;++n){property[count]=reinterpret_cast<void*>(first+n*0xb30+0x740+0x188);saved[count]=F(property[count],0x50,unsigned);++count;}
        }
        applied=true;
        for(unsigned k=0;k<count;++k)if(saved[k]){++changed;reinterpret_cast<void(*)(void*,unsigned)>(image+0x294bfc0)(property[k],0);}
        return true;
    }
    ~OnePass(){if(applied)for(unsigned k=count;k-->0;)if(saved[k])reinterpret_cast<void(*)(void*,unsigned)>(image+0x294bfc0)(property[k],saved[k]);}
};
void render(void*context,void*services,void*output,void*event,void*old_track){
    resample_fault_stage(2);
    std::fprintf(stderr,"RESAMPLE MINI_ENTRY context=%p services=%p output=%p event=%p track=%p\n",context,services,output,event,old_track);
    std::string path;unsigned expected_source=128;
    void*engine=F(services,0x5c0,void*);
    {
        std::lock_guard<std::mutex>lock(job_lock);
        if(armed&&services==job_services){path=job_path;expected_source=job_source;armed=false;}
    }
    if(path.empty()){
        std::fprintf(stderr,"RESAMPLE MINI_UNOWNED original_single_renderer=1\n");
        reinterpret_cast<void(*)(void*,void*,void*,void*,void*)>(image+0x18b9a50)(context,services,output,event,old_track);return;
    }
    job_state.store(2,std::memory_order_release);
    resample_fault_stage(3);
    struct End {~End(){job_state.store(3,std::memory_order_release);}}end;
    /* x1 at18b9df0 is the native export SERVICE context, not settings.
     * Stock18b9ca0 reads output rate at render context+398. */
    Mini m{};m.context=context;m.engine=engine;m.rate=F(context,0x398,unsigned);
    void*things=m.engine?F(m.engine,0x3c0,void*):nullptr,*program=things?F(things,0xb8,void*):nullptr;
    m.track=things?F(things,0x140,void*):nullptr;
    unsigned midi=event?F(event,48,unsigned char):128;
    int pad=program&&midi<128?F(program,0x14a8+midi*4,int):-1;
    if(mini||!m.track||F(m.track,0xbb8,void*)!=program||pad<0||pad>=128||(unsigned)pad!=expected_source||F(program,0x12a8+pad*4,int)!=(int)midi||m.rate<8000||m.rate>192000){cancel(m,1);return;}
    resample_fault_stage(4);
    void*mixer=F(services,0x5a0,void*);
    std::fprintf(stderr,"RESAMPLE MINI_CONTEXT pad=%d rate=%u same_track=%u track_muted=%u track_gain=%.6g mixer_track_matches=%u tap=%u\n",pad,m.rate,m.track==old_track,F(m.track,0x4f0,unsigned char)&1,(double)F(m.track,0xb00,float),mixer&&F(mixer,0x138,void*)==static_cast<char*>(m.track)+0x228,uses_tap(m));
    uintptr_t service_vtable=F(services,0,uintptr_t);
    std::fprintf(stderr,"RESAMPLE MINI_ROUTE service_vtable=%llx process=%llx input_muted=%u note_type=%u note_midi=%u velocity=%.6g\n",
        (unsigned long long)(service_vtable-image.load()),
        (unsigned long long)(F(reinterpret_cast<void*>(service_vtable),16,uintptr_t)-image.load()),
        F(m.track,0x878,unsigned char),F(event,8,unsigned char),midi,(double)F(event,16,float));
    PadGroup group;
    if(!group.begin(program,(unsigned)pad)){cancel(m,1);return;}
    for(unsigned k=0;k<group.count;++k){
        uint64_t frames=source_floor(program,group.pad[k],m.rate);if(frames>m.floor)m.floor=frames;
        std::fprintf(stderr,"RESAMPLE MINI_PAD index=%u pad=%u raw_frames=%llu muted=%u links=%u\n",k,group.pad[k],(unsigned long long)frames,F(group.instrument[k],0x2e0,unsigned char)&1,F(group.instrument[k],0x234d,unsigned char)&1);
    }
    resample_fault_stage(5);
    std::fprintf(stderr,"RESAMPLE MINI_SOURCE frames=%llu\n",(unsigned long long)m.floor);
    /*18b9ca0's span is derived from the CURRENT SEQUENCE, not the processed
     * sample. It must neither truncate a long sample nor prolong a short one.
     * Loop uses the same finite raw-source floor as an ordinary sample. */
    m.minimum=m.floor;
    if(!m.floor||!m.minimum||m.minimum>UINT64_C(192000)*60*60){cancel(m,1);return;}
    std::string parent=path.substr(0,path.find_last_of('/'));if(parent.empty())parent="/";
    struct statvfs space{};
    if(statvfs(parent.c_str(),&space)||(__uint128_t)space.f_bavail*space.f_frsize<(__uint128_t)m.minimum*8+1024*1024){cancel(m,5);return;}
    std::memcpy(m.note,event,sizeof(m.note));F(m.note,0,int64_t)=0;
    /* Hold the private note throughout the finite export budget. Natural
     * one-pass sample completion, followed by stable silence, ends export.
     * Do not send note-off at raw length: lower pitch may still be playing. */
    F(m.note,40,int64_t)=(int64_t)(m.minimum*16+(uint64_t)m.rate*31);
    struct Scope{Mini*previous;explicit Scope(Mini*p):previous(mini){mini=p;}~Scope(){mini=previous;}}scope(&m);
    resample_fault_stage(6);
    /* Loop belongs to each layer's SliceProperties, not the Instrument's
     * BooleanProperty at1fc8 (private audition also changes that separate
     * control). Native294a380/294bca0 map SliceData.loopMode(+32) to the
     * property atSlice+188, value+50. Preserve all other pad settings. */
    OnePass one_pass;
    if(!one_pass.begin(group)){cancel(m,1);return;}
    std::fprintf(stderr,"RESAMPLE MINI_BEGIN pad=%d group=%u raw_floor=%llu native_span=%llu\n",pad,group.count,(unsigned long long)m.floor,(unsigned long long)F(context,0x9f8,uint64_t));
    /* The device's Source4 job has no resolved direct output and leaves
     * include-master (+4a0) disabled.18b97c0 then flushes a writer with no
     * registered audio at all. Select its existing master-output branch for
     * this private job only; preserve the caller's settings on every exit. */
    struct OutputRoute {
        void*context;unsigned char saved;bool changed=false;
        explicit OutputRoute(void*c):context(c),saved(F(c,0x4a0,unsigned char)){}
        ~OutputRoute(){if(changed)F(context,0x4a0,unsigned char)=saved;}
    }route(context);
    if(!output){cancel(m,6);return;}
    m.tap_to_primary=!F(output,8,void*);
    if(m.tap_to_primary){
        void*owner=F(context,0xa0,void*),*router=owner?F(owner,0x130,void*):nullptr;
        /* Real native resolver18ccbc0: destination2/subIndex0 maps the first
         * physical stereo AudioBuffer if that output exists. No routing,
         * faders, pad properties or user project settings are changed. */
        if(router&&F(router,0x140,unsigned)>0){
            alignas(8) unsigned route[8]={2,0,3,0,0,1,0x3f800000,0};
            m.physical_output=reinterpret_cast<void*(*)(void*,const void*)>(image+0x18ccbc0)(router,route);
            if(!m.physical_output){cancel(m,6);return;}
        }
    }
    if(!F(output,8,void*)&&!uses_tap(m)){
        void*owner=F(context,0xa0,void*);
        if(F(context,0x3f0,unsigned)!=4||!owner||!F(owner,0x130,void*)){cancel(m,6);return;}
        F(context,0x4a0,unsigned char)=route.saved|1u;route.changed=true;
    }
    std::fprintf(stderr,"RESAMPLE MINI_OUTPUT source=%u direct=%u tap=%u enabled_master=%u primary_from_master=%u physical_pair=%u\n",
        F(context,0x3f0,unsigned),F(output,8,void*)!=nullptr,uses_tap(m),route.changed,m.tap_to_primary,m.physical_output!=nullptr);
    std::string actual_path;
    void*writer=F(context,0xa38,void*);const char*actual=writer?F(writer,0xd8,const char*):nullptr;
    if(actual&&strnlen(actual,4096)<4096)actual_path=actual;
    m.delegated=true;resample_fault_stage(7);
    reinterpret_cast<void(*)(void*,void*,void*)>(image+0x18b9ee0)(context,services,output);
    resample_fault_stage(11);
    auto written=resample_file_probe(path.c_str());
    std::fprintf(stderr,"RESAMPLE PRIMARY_CLOSED valid=%u bytes=%u peak=%.9g path=%s\n",written.valid,written.bytes,written.peak,path.c_str());
    if(!m.failed&&m.tail.state!=RFT_QUIET)failure.store(4,std::memory_order_release);
    if(!failure.load(std::memory_order_acquire)&&!actual_path.empty()&&actual_path!=path){
        auto stem=resample_file_probe(actual_path.c_str());
        std::lock_guard<std::mutex>lock(job_lock);
        bool silent=stem.valid&&stem.bytes&&stem.scanned==stem.bytes&&stem.peak==0;
        bool captured=!silent&&job_path==path&&job_output.capture(path,actual_path,m.rate,m.minimum);
        if(!captured)failure.store(silent?10:8,std::memory_order_release);
        std::fprintf(stderr,"RESAMPLE OUTPUT_CAPTURE captured=%u valid=%u bytes=%u peak=%.9g requested=%s actual=%s\n",captured,stem.valid,stem.bytes,stem.peak,path.c_str(),actual_path.c_str());
    }
    std::fprintf(stderr,"RESAMPLE MINI_END started=%u loop_one_pass=%u state=%u failure=%u blocks=%u direct=%u tap=%u primary=%u dispatches=%u blocked=%u scanned=%llu peak=%.9g\n",m.started,one_pass.changed,m.tail.state,failure.load(),m.blocks,m.direct_blocks,m.tap_blocks,m.primary_blocks,m.dispatches,m.blocked,(unsigned long long)m.tail.scanned,(double)m.peak);
}
void emit_jump(void*page,unsigned at,uintptr_t target,bool unsign){
    uint32_t code[]={unsign?0xd50323bfu:0xd503245fu,0x58000070,0xd61f0200,0xd503201f};
    std::memcpy(static_cast<char*>(page)+at,code,16);F(page,at+16,uintptr_t)=target;
}
void*relay_page(uintptr_t base){
    for(uintptr_t n=1;n<=60;++n){if(base<n*0x100000+65536)break;void*hint=reinterpret_cast<void*>(base-n*0x100000);
        void*p=mmap(hint,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(p==MAP_FAILED)continue;
        if(p==hint)return p;
        munmap(p,4096);
    }return nullptr;
}
}
extern "C" const char*resample_fast_native_error(){
    if(job_state.load(std::memory_order_acquire)!=3)return "Private sample render did not finish";
    switch(failure.load(std::memory_order_acquire)){case 0:return nullptr;case 1:return "Cannot prepare private sample sequence";case 2:return "Invalid audio in sample render";case 3:return "Effect tail did not decay within render budget";case 5:return "Insufficient space for sample render";case 6:return "Render output missing: try again";case 8:return "Render file missing: try again";case 9:return "Cannot finalize rendered sample file";case 10:return "Sample render is silent; destination unchanged";default:return "Sample sequence did not finish";}
}
extern "C" int resample_fast_native_arm(void*services,unsigned source,const char*path){
    if(!services||source>=128||!path||path[0]!='/'||strnlen(path,4096)>=4096)return 0;
    std::lock_guard<std::mutex>lock(job_lock);
    job_output.reset();job_path=path;job_services=services;job_source=source;armed=true;failure.store(0,std::memory_order_release);job_state.store(1,std::memory_order_release);return 1;
}
extern "C" int resample_fast_native_prepare_import(const char*path){
    std::lock_guard<std::mutex>lock(job_lock);
    if(!path||job_path!=path||job_state.load(std::memory_order_acquire)!=3||failure.load(std::memory_order_acquire))return 0;
    if(!job_output.pending())return 1;
    bool ok=job_output.commit(path);
    if(!ok)failure.store(9,std::memory_order_release);
    auto p=resample_file_probe(path);
    std::fprintf(stderr,"RESAMPLE OUTPUT_FINALIZED success=%u valid=%u bytes=%u peak=%.9g path=%s\n",ok,p.valid,p.bytes,p.peak,path);
    return ok;
}
extern "C" int resample_fast_native_ready(uintptr_t base){return base&&installed.load(std::memory_order_acquire)&&image.load(std::memory_order_acquire)==base;}
extern "C" int resample_fast_native_install(uintptr_t base){
    std::lock_guard<std::mutex>lock(install_lock);if(image)return image==base;
    if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
    struct Patch {uintptr_t offset;uint32_t expected;uintptr_t target;unsigned relay;bool call;};
    Patch patches[]={{0x18b9df0,0x97ffff18,(uintptr_t)render,0,true},
        {0x18b9f7c,0x97ffef11,(uintptr_t)prepare,32,true},
        {0x18b9874,0x94216ce3,(uintptr_t)frame,64,true},
        {0x18b98e0,0x97fffd3c,(uintptr_t)write_block,96,true},
        {0x211a234,0xa9bc7bfd,(uintptr_t)input,128,false},
        {0x2119d04,0xd10583ff,(uintptr_t)dispatch,160,false},
        {0x18b9a38,0x97fffeae,(uintptr_t)tap_block,192,true}};
    void*p=relay_page(base);if(!p)return 0;
    uintptr_t pages[7];unsigned page_count=0;
    for(unsigned i=0;i<7;++i){auto&v=patches[i];uint32_t current=F(reinterpret_cast<void*>(base),v.offset,uint32_t);
        if(v.call&&current!=v.expected){munmap(p,4096);return 0;}
        if(!v.call){
            if(F(reinterpret_cast<void*>(base),v.offset-4,uint32_t)!=0xd503233f){munmap(p,4096);return 0;}
            unsigned at=224+(i-4)*48;uintptr_t continuation=base+v.offset+4;
            if(current==v.expected){
                uint32_t prefix[]={0xd503245f,0xd503233f,current,0x58000070,0xd61f0200,0xd503201f};
                std::memcpy(static_cast<char*>(p)+at,prefix,sizeof(prefix));F(p,at+24,uintptr_t)=continuation;
            }else if((current&0xfc000000u)==0x14000000u){
                int32_t d=(int32_t)(current<<6)>>6;uintptr_t bridge=base+v.offset+(int64_t)d*4;
                if(bridge<base+0x6d3d000||bridge>=base+0x6d42000){munmap(p,4096);return 0;}
                uint32_t prefix[]={0xd503245f,0xd503233f,0x58000090,0xd61f0200,0xd503201f,0xd503201f};
                std::memcpy(static_cast<char*>(p)+at,prefix,sizeof(prefix));F(p,at+24,uintptr_t)=bridge;
            }else{munmap(p,4096);return 0;}
            if(i==4)input_original=reinterpret_cast<Input>(static_cast<char*>(p)+at);else dispatch_original=reinterpret_cast<Dispatch>(static_cast<char*>(p)+at);
        }
        emit_jump(p,v.relay,v.target,!v.call);
        intptr_t delta=reinterpret_cast<uintptr_t>(p)+v.relay-(base+v.offset);
        if(delta%4||delta<-(INT64_C(1)<<27)||delta>=(INT64_C(1)<<27)){munmap(p,4096);return 0;}
        uintptr_t page=(base+v.offset)&~(uintptr_t)4095;bool found=false;for(unsigned j=0;j<page_count;++j)found|=pages[j]==page;if(!found)pages[page_count++]=page;
    }
    __builtin___clear_cache(static_cast<char*>(p),static_cast<char*>(p)+320);
    if(mprotect(p,4096,PROT_READ|PROT_EXEC)){munmap(p,4096);return 0;}
    for(unsigned i=0;i<page_count;++i)if(mprotect(reinterpret_cast<void*>(pages[i]),4096,PROT_READ|PROT_WRITE)){
        for(unsigned j=0;j<i;++j)if(mprotect(reinterpret_cast<void*>(pages[j]),4096,PROT_READ|PROT_EXEC))_exit(121);
        munmap(p,4096);return 0;
    }
    image=base;
    for(auto&v:patches){intptr_t delta=reinterpret_cast<uintptr_t>(p)+v.relay-(base+v.offset);F(reinterpret_cast<void*>(base),v.offset,uint32_t)=(v.call?0x94000000u:0x14000000u)|(uint32_t(delta/4)&0x03ffffffu);__builtin___clear_cache(reinterpret_cast<char*>(base+v.offset),reinterpret_cast<char*>(base+v.offset+4));}
    for(unsigned i=0;i<page_count;++i)if(mprotect(reinterpret_cast<void*>(pages[i]),4096,PROT_READ|PROT_EXEC))_exit(121);
    installed.store(true,std::memory_order_release);
    std::fprintf(stderr,"RESAMPLE FAST_PATCH_READY bootstrap_required=1\n");
    return 1;
}
