int main(void){
 static char fake[0x4000];files=fake;FIELD(files,0x1388,int)=FILE_SAMPLES;
 static const char*names[]={"/media/MPC/Samples/Drums/Kicks/Kick Boom 01.wav","/media/MPC/Samples/Drums/Kicks/Kick Dusty 808.wav",
  "/media/MPC/Samples/Drums/Kicks/Kick Tight.wav","/media/MPC/Samples/Loops/Kickin Groove 92bpm.wav","/media/MPC/Samples/Drums/Kicks/Kick Vinyl Long Tail Very Long Name.wav",
  "/media/MPC/Samples/Drums/Snares/Snare Crack.wav","/media/MPC/Samples/Kits/BoomBap/BoomBap-Kik-BTB MD.wav"};
 entry_count=sizeof names/sizeof*names;for(unsigned i=0;i<entry_count;++i){entries[i].file=(uint64_t)(uintptr_t)names[i];entries[i].engaged=1;}
 search=1;search_where=WHERE_ALL;apply_where();volume_ready=1;volume_percent=80;library_root_found=1;snprintf(search_root,sizeof search_root,"/media/MPC/Samples");
 phone_init(&query,64);phone_enable(&query,1);phone_sync(&query,"ki",2);phone_utf8(&query,query_text,sizeof query_text);strcpy(scan_query,query_text);filter();
 PREVIEW_BEGIN("search-1-query.ops");search_paint(0);PREVIEW_END();
 phone_sync(&query,"zz",2);phone_utf8(&query,query_text,sizeof query_text);strcpy(scan_query,query_text);filter();
 PREVIEW_BEGIN("search-2-nomatch.ops");search_paint(0);PREVIEW_END();
 phone_sync(&query,"kick",4);phone_utf8(&query,query_text,sizeof query_text);strcpy(scan_query,query_text);filter();
 results=1;selected=4;result_pad_ready=1;result_pad=3;selected_audio=1;volume_ready=1;volume_percent=80;search_preview_on=1;
 PREVIEW_BEGIN("search-3-list.ops");search_paint(0);PREVIEW_END();
 return 0;}
