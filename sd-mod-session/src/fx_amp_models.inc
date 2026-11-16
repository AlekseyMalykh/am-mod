/* Voicings on the existing AIR Amp Sim, inspired by guitar and bass amplifier families.
 * These are not measured circuit models or cabinet IR captures.
 * AIR exposes eleven parameters. Cell 31 is reserved project metadata and is
 * never passed to that processor (fx_native queries its actual parameter count).
 * Zero retains the original Amp Sim; legacy quarters retain the four guitar voicings; unused eighths identify bass variants. */
struct fx_amp_voice {const char*name;float p[11];float marker;};
static const struct fx_amp_voice amp_voices[]={
 {"Original",{0,0,0,0,.5f,.5f,.5f,.5f,.5f,.1f,1},0},
 {"FD Clean",{.12f,.05f,.12f,0,.55f,.42f,.45f,.62f,.47f,.5f,1},.25f},
 {"VX Chime",{.30f,.20f,.65f,.10f,.42f,.58f,.62f,.65f,.42f,.5f,1},.5f},
 {"MR Crunch",{.62f,.45f,.25f,.12f,.55f,.64f,.48f,.58f,.36f,.7f,1},.75f},
 {"MB Modern",{.82f,.72f,.13f,.08f,.64f,.35f,.40f,.62f,.28f,.7f,1},1},
 {"SV Bass",{.35f,.35f,.04f,.08f,.62f,.58f,.28f,.40f,.43f,.9f,1},.125f},
 {"BM Round",{.20f,.12f,.04f,.02f,.65f,.46f,.30f,.42f,.47f,.8f,1},.375f},
 {"GK Punch",{.28f,.08f,.15f,.02f,.57f,.62f,.38f,.60f,.44f,.9f,1},.625f}
};
unsigned fx_amp_model_count(void){return sizeof(amp_voices)/sizeof(amp_voices[0]);}
unsigned fx_amp_model_index(const struct fx_stage*s)
{
    if(!s||s->kind!=FX_STAGE_NATIVE||s->native_id!=0x61464153u||!isfinite(s->parameter[31]))return 0;
    for(unsigned i=0;i<fx_amp_model_count();++i)
        if(fabsf(s->parameter[31]-amp_voices[i].marker)<.0001f)return i;
    return 0;
}
const char*fx_amp_model_name(unsigned index){return amp_voices[index<fx_amp_model_count()?index:0].name;}
int fx_amp_model_apply(struct fx_stage*s,unsigned index)
{
    if(!s||s->kind!=FX_STAGE_NATIVE||s->native_id!=0x61464153u||index>=fx_amp_model_count())return 0;
    memcpy(s->parameter,amp_voices[index].p,sizeof amp_voices[index].p);s->parameter[31]=amp_voices[index].marker;return 1;
}
float fx_catalog_parameter_default(const struct fx_stage*s,unsigned index)
{
    const struct fx_catalog_entry*e=s?fx_catalog_find(s->native_id):NULL;
    if(!e||index>=e->count)return 0;
    if(s->native_id==0x61464153u&&index<11)return amp_voices[fx_amp_model_index(s)].p[index];
    return e->defaults[index];
}

float fx_catalog_parameter_value(const struct fx_stage*s,unsigned index)
{
    if(!s)return 0;
    if(index==FX_CONTROL_AMP_MODEL)return fx_amp_model_index(s)/(float)(fx_amp_model_count()-1);
    return index<FX_CHAIN_PARAMETERS?s->parameter[index]:0;
}
