int main(void){
 base=0;
 for(unsigned t=0;t<MOD_TAB_COUNT;++t){mod_tab=t;mod_row=t==1?2:0;mod_edit=t==1;char n[48];snprintf(n,sizeof n,"settings-%u.ops",t);PREVIEW_BEGIN(n);mod_settings_paint(0);PREVIEW_END();}
 mod_tab=2;mod_row=MUSIC_SERVICE_COUNT;mod_edit=0;PREVIEW_BEGIN("settings-5-restore-row.ops");mod_settings_paint(0);PREVIEW_END();
 mod_reset_confirm=1;PREVIEW_BEGIN("settings-6-confirm.ops");mod_settings_paint(0);PREVIEW_END();
 /* Lights page footer as drawn in the Sample screen, Degrees selected. */
 mod_reset_confirm=0;focus=0;
 PREVIEW_BEGIN("settings-7-lights-degrees.ops");
 colour(0,0xff000000);rect(0,0,0,320,198);
 cell(0,0,"Layout","Degrees",0,1,1,3,MUSIC_LAYOUT_CHOICES);cell(0,1,"Tonic","C (P)",0,1,1,1,13);cell(0,2,"Scale","Major (P)",0,1,1,1,9);
 PREVIEW_END();
 focus=1;PREVIEW_BEGIN("settings-8-lights-roles.ops");colour(0,0xff000000);rect(0,0,0,320,198);
 cell(0,0,"Transpose16","+0",1,1,1,24,49);cell(0,1,"Role","V",1,1,1,2,5);cell(0,2,"Colour","Yellow",1,1,1,8,12);PREVIEW_END();
 return 0;}
