/* Production palette UI state; native drawing is deliberately not invoked. */
#include "src/music_ui.c"
#include <assert.h>
#include <stdlib.h>
int main(void){
 uint64_t transfer=0;assert(!music_ui_transfer_light(0,&transfer));
 atomic_store(&exit_blocked,1);assert(!music_ui_transfer_light(0,&transfer));atomic_store(&usb_transfer_confirmed,1);
 for(unsigned pad=0;pad<16;++pad)assert(music_ui_transfer_light(pad,&transfer)&&transfer==usb_drive_amber_light());
 assert(!music_ui_transfer_light(16,&transfer));atomic_store(&exit_blocked,0);assert(!music_ui_transfer_light(0,&transfer));atomic_store(&usb_transfer_confirmed,0);
 char dir[]="/tmp/am-palette-ui-XXXXXX";assert(mkdtemp(dir));
 assert(music_lighting_preferences_open(dir));
 assert(!music_hide_play_cursor()&&music_pad_cursor_visible(3));
 assert(music_set_hide_play_cursor(1)&&!music_set_hide_play_cursor(2));
 assert(!music_pad_cursor_visible(3)&&music_pad_cursor_visible(2)&&music_pad_cursor_visible(4));
 music_reset();assert(music_hide_play_cursor());
 assert(music_lighting_preferences_open(dir)&&music_hide_play_cursor());
 mod_tab=3;mod_row=1;assert(mod_rows()==2&&hidecursor_row(mod_row)&&mod_value()==1);
 assert(music_mod_restore_recommended(3)&&!music_hide_play_cursor());
 mod_tab=0;mod_row=0;

 assert(music_palette_save(31,0xff0088,"Last in B"));
 assert(palette_select(127,31,1,1)==13); /* Backwards over empty slots. */
 assert(palette_select(1,13,1,1)==31);
 palette_load(31);key_page=5;palette_edit=1;++palette_ticket;
 palette_publish();uint64_t ticket=music_ui_palette_ticket(),light;
 assert(music_ui_button_owns(0x23));
 palette_drafts[31].rgb=0x123456;palette_changes=1u<<31;
 palette_bank_step(1);assert(palette_index==15&&key_page==5&&palette_edit);
 palette_bank_step(-1);assert(palette_index==31&&palette_drafts[31].rgb==0x123456&&palette_changes==(1u<<31));
 assert(ticket&&music_ui_palette_light(15,&light));
 assert(!music_ui_palette_light(16,&light));
 music_ui_palette_pad(0,1,0,0,ticket);assert(palette_index==16);
 music_ui_palette_pad(15,1,0,0,ticket);assert(palette_index==31);
 music_ui_palette_pad(0,1,0,0,ticket-1);assert(palette_index==31);
 palette_index=0;palette_publish();ticket=music_ui_palette_ticket();
 music_ui_palette_pad(15,1,0,0,ticket);assert(palette_index==15);
 palette_drafts[31].rgb=0x00ff77;palette_changes=1u<<31;
 assert(palette_cell_visible(31));
 assert(music_palette_apply(palette_drafts,palette_changes));
 palette_load(31);assert(music_palette_rgb(31)==0x00ff77&&!palette_changes);
 key_page=0;palette_publish();assert(!music_ui_palette_ticket());
 assert(!music_ui_button_owns(0x23));
 puts("PASS production palette UI: bank-relative physical pads, only 16 LEDs, stale tickets, slot 31 save and close");
 return 0;
}
