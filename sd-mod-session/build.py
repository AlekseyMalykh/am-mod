from pathlib import Path
from hashlib import sha256
import json,struct,subprocess,zipfile,sys
PHONE='--phone' in sys.argv
PHONE_REV='r16d-sample-input-preview'
R=Path(__file__).resolve().parent
W=R.parent
B=W/'sd-k-value-peek'
def digest(b): return sha256(b).hexdigest()
def run(*a): subprocess.run(a,cwd=R,check=True)
def branch(a,b,call=False):
 d=b-a
 assert d%4==0 and -(1<<27)<=d<(1<<27)
 return struct.pack('<I',(0x94000000 if call else 0x14000000)|((d//4)&0x3ffffff))
def main():
 stock=(W/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample').read_bytes()
 assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
 r3=json.loads((B/'build-info.json').read_text())
 assert r3['revision']=='r3-yellow-preview-white-edit'
 assert digest((B/'autorun/autorun.sh').read_bytes())=='b25b7cbc299f0a7ad6a1ab0b15756b5aa33e8a53ecde3ac741f32ff4e8afd3be'
 for unit in ['values']:
  extra=['-DNOTE_UI'] if PHONE else []
  link=[]
  if PHONE:
   from phone_build import FOOTER_VA
   link=['--defsym=note_footer_bridge='+hex(FOOTER_VA)]
  run('aarch64-linux-gnu-gcc',*extra,'-c','src/'+unit+'.S','-o','build/'+unit+'.o')
  run('aarch64-linux-gnu-ld',*link,'-T','src/'+unit+'.ld','build/'+unit+'.o','-o','build/'+unit+'.elf')
  run('aarch64-linux-gnu-objcopy','-O','binary','--only-section=.text','build/'+unit+'.elf','build/'+unit+'.bin')
 project_flags=['-DNOTE_UI'] if PHONE else []
 project_link=[]
 if PHONE:
  from phone_build import PROJECT_REQUEST_VA
  project_link=['--defsym=music_project_bridge='+hex(PROJECT_REQUEST_VA)]
 run('aarch64-linux-gnu-gcc',*project_flags,'-c','src/project.S','-o','build/project.o')
 run('aarch64-linux-gnu-ld',*project_link,'-T','src/project.ld','build/project.o','-o','build/project.elf')
 run('aarch64-linux-gnu-objcopy','-O','binary','--only-section=.text','build/project.elf','build/project.bin')
 symbols=subprocess.check_output(['aarch64-linux-gnu-nm',str(R/'build/project.elf')],text=True)
 syms={line.split()[2]:int(line.split()[0],16) for line in symbols.splitlines()}
 project=(R/'build/project.bin').read_bytes()
 assert len(project)<0x200
 value_symbols=subprocess.check_output(['aarch64-linux-gnu-nm',str(R/'build/values.elf')],text=True)
 syms.update({line.split()[2]:int(line.split()[0],16) for line in value_symbols.splitlines()})
 values=(R/'build/values.bin').read_bytes();assert len(values)<=4096
 assert syms['values_paint']==0x6d3b800
 assert struct.unpack_from('<H',stock,16)[0]==3, 'PIE input expected'
 audit=json.loads((R/'ui-audit.json').read_text()); screen_targets=[x['vptr'] for x in audit['screens']]; assert len(screen_targets)==26 and len(audit['shift_pairs'])==19
 table=syms['values_screen_offsets']; table_offset=table-syms['values_paint']
 for i,target in enumerate(screen_targets):
  entry=table+i*8
  delta=struct.unpack_from('<q',values,table_offset+i*8)[0]
  assert entry+delta==target, 'screen table must use entry-relative PIE offsets'
  assert delta!=target, 'raw link-time pointer is invalid in injected PIE data'
 pairs=syms['ui_shift_pairs']
 for i,row in enumerate(audit['shift_pairs']):
  entry=pairs+i*24; delta=struct.unpack_from('<q',values,entry-syms['values_paint'])[0]
  assert entry+delta==int(row['screen_vptr'],16), 'relative Shift pair screen pointer'
 chunks=[('project-code.bin',0x6d3b400,project),
 ('project-append.bin',0x1642f54,branch(0x1642f54,syms['project_append'],True)),
 ('project-dispatch.bin',0x1643908,branch(0x1643908,syms['project_dispatch'])),
 ('project-rows.bin',0x1644ff4,struct.pack('<I',0xd28000c1 if PHONE else 0xd28000a1)),
 ('values-code.bin',0x6d3b800,values),
 ('values-render.bin',0x15cd784,branch(0x15cd784,syms['values_paint'])),
 ('values-label-render.bin',0x15cf404,branch(0x15cf404,syms['values_label_paint'])),
 ('ui-indicator-render.bin',0x3b7a714,branch(0x3b7a714,syms['ui_indicator_paint'])),
 ('ui-indicator-bounds.bin',0x1160fc4,branch(0x1160fc4,syms['ui_indicator_bounds'])),
 ('session-phdr.bin',0xd0,struct.pack('<QQ',0x6d3b800+len(values),0x6d3b800+len(values)))]
 if PHONE:
  from phone_build import build as build_phone
  phone_chunks,phone_symbols=build_phone(stock,run,branch)
  chunks+=phone_chunks;syms.update(phone_symbols)
  chunks=[(n,o,struct.pack('<QQ',0x6d43000,0x6d43000) if n=='session-phdr.bin' else p) for n,o,p in chunks]
 assert stock[0x15cd780:0x15cd788]==bytes.fromhex('3f2303d5ff4304d1'), 'Preserve original PACIASP; displace only SUB SP'
 assert stock[0x3b7a710:0x3b7a718]==bytes.fromhex('3f2303d5fd7bbda9'), 'indicator PAC/stack contract'
 assert stock[0x1160fc0:0x1160fc8]==bytes.fromhex('5f2403d5e303022a'), 'bounds BTI/argument contract'
 assert stock[0x15cf400:0x15cf408]==bytes.fromhex('3f2303d5ff0305d1'), 'Label PACIASP preserved'
 assert stock[0x3b7d360:0x3b7d364]==branch(0x3b7d360,0xff9370,True)
 assert stock[0x1642f54:0x1642f58]==branch(0x1642f54,0x3b71a60,True)
 assert stock[0x1643908:0x164390c]==struct.pack('<I',0x71000c1f)
 assert stock[0x1644ff4:0x1644ff8]==struct.pack('<I',0xd2800081)
 assert not any(stock[0x6d3aff8:0x6d43bb0])
 ordered=sorted((o,o+len(p)) for _,o,p in chunks)
 assert all(a[1]<=b[0] for a,b in zip(ordered,ordered[1:])), 'patch ranges overlap'
 assert 0x6d3b800+len(values)<0x6d43bb0, 'RX cave must not reach RW data'
 header=['/* Generated from pinned firmware. Only fixed RAM path is writable. */','#include <stddef.h>','struct patch { off_t offset; size_t size; const unsigned char *before,*after; };']
 patched=bytearray(stock)
 def array(name,data):return 'static const unsigned char '+name+'[]={'+','.join('0x%02x'%x for x in data)+'};'
 for i,(_,o,p) in enumerate(chunks):
  header += [array('before'+str(i),stock[o:o+len(p)]),array('after'+str(i),p)]
  patched[o:o+len(p)]=p
 assert patched[0x6d43bb0:]==stock[0x6d43bb0:], 'RW segment must remain identical'
 assert patched[0x3b7d360:0x3b7d364]==stock[0x3b7d360:0x3b7d364], 'native value-change timer remains untouched'
 header+=['static const struct patch patches[]={'+','.join('{%d,%d,before%d,after%d}'%(o,len(p),i,i) for i,(_,o,p) in enumerate(chunks))+'};']
 (R/'src/patches.h').write_text('\n'.join(header)+'\n')
 run('aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Wl,-z,noexecstack','src/apply_session.c','-o','build/apply-session')
 run('aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Wl,-z,noexecstack','src/stage_data.c','-o','build/stage-data')
 if not PHONE:
  run('aarch64-linux-gnu-gcc','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Wl,-z,noexecstack,-z,relro,-z,now','src/save_io_trace.c','-ldl','-o','build/save-io.so')
 out=R/'autorun';out.mkdir(exist_ok=True)
 for n in ['xdg-dbus-proxy','proxy-policy.args']:(out/n).write_bytes((B/'autorun'/n).read_bytes())
 helper=(R/'build/apply-session').read_bytes()
 if (out/'apply-session').exists():(out/'apply-session').chmod(0o700)
 (out/'apply-session').write_bytes(helper)
 stage=(R/'build/stage-data').read_bytes()
 if (out/'stage-data').exists():(out/'stage-data').chmod(0o700)
 (out/'stage-data').write_bytes(stage)
 library='phone-input.so' if PHONE else 'save-io.so'
 trace=(R/'build'/library).read_bytes()
 (out/library).write_bytes(trace)
 text=(R/'src/autorun.template.sh').read_text()
 text=text.replace('SAVE_IO_TRACE_SHA',digest(trace))
 if PHONE:
  text=text.replace('save-io.so','phone-input.so')
  text=text.replace('# A bounded diagnostic interceptor is read-only in the clone namespace.','# The keyboard library is read-only in the clone namespace.')
  text=text.replace('SAVE_IO_DIAGNOSTICS r9h max_records1024 RAM_only','PHONE_INPUT r15f Preview_master_gain_Chop_manager bounded_UI_FIFO no_background_observer')
 text=text.replace('STAGE_HELPER_SHA',digest(stage))
 begin=text.index('# Apply only to the just-verified copy')
 end=text.index('[ "$(hash "$DIR/xdg-dbus-proxy")"',begin)
 inject="""# Apply the fixed patch set to the verified RAM copy before stopping stock.
chmod 700 "$R/MPC-Sample"
[ "$(hash "$DIR/apply-session")" = "HELPER_SHA" ] || fail WRONG_SESSION_HELPER
cp "$DIR/apply-session" "$R/apply-session"
chmod 500 "$R/apply-session"
[ "$(hash "$R/apply-session")" = "HELPER_SHA" ] || fail SESSION_HELPER_COPY_MISMATCH
"$R/apply-session" >> "$R/events.txt" 2>&1 || fail SESSION_PATCH_FAILED
chmod 500 "$R/MPC-Sample"
[ "$(hash "$R/MPC-Sample")" = "$PATCH_SHA" ] || fail SESSION_HASH_MISMATCH
log "MOD_SESSION_PATCH_VERIFIED sha256=$PATCH_SHA"
"""
 text=text[:begin]+inject.replace('HELPER_SHA',digest(helper))+text[end:]
 text=text.replace(r3['patched_copy_sha256'],digest(patched))
 text=text.replace('experimental K-value peek in a RAM copy','manual Mod Off in Project menu; RAM copy only')
 (out/'mod-launcher.sh').write_text(text,newline='\n')
 entry=(R/'src/entry.template.sh').read_text().replace('CONTROLLER_SHA256',digest(text.encode()))
 if PHONE:entry=entry.replace('r9h-save-path-diagnostics',PHONE_REV).replace('=== r9h entry','=== r15f entry')
 assert 'CONTROLLER_SHA256' not in entry
 (out/'autorun.sh').write_text(entry,newline='\n')
 for n,o,p in chunks:(out/n).write_bytes(p)
 payload_names=sorted(['apply-session','stage-data',library,'mod-launcher.sh','autorun.sh','autorun.json','autorun.check','xdg-dbus-proxy','proxy-policy.args']+[n for n,_,_ in chunks])
 assert len(set(payload_names))==len(payload_names)
 manifest={'value0':{'scripts':[{'path':'autorun.sh','askQuestion':True,'message':'Start r9h save diagnostics? Try one short sample and project on External with new names. RAM-only file log. Exit: Project > Mod Off.','yesButtonText':'Start mod','noButtonText':'Cancel'}],'files':[]}}
 if PHONE:
  manifest['value0']['scripts'][0]['message']='Start r16d? Chop preview uses ordinary Sample receiver; Levels preview retained. Original event is silent in sequence while editing. Amber original, cyan choice. B3 Apply; B1/B2 Cancel. Insert after stock loads. Project > Mod Off to exit.'
 raw=(json.dumps(manifest,indent=2)+'\n').encode()
 (out/'autorun.json').write_bytes(raw)
 (out/'autorun.check').write_text(digest(raw+entry.encode()+b'f7bed1c9-7e90-4731-9194-c996666b4331'))
 archive=W/('mpc-sample-event-preview-r16d.zip' if PHONE else 'mpc-sample-mod-session.zip')
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  for n in payload_names:z.write(out/n,'autorun/'+n)
  z.write(R/('R16D_RU.md' if PHONE else 'README_RU.md'),'README_RU.md')
  z.write(R/'UI_AUDIT_RU.md','UI_AUDIT_RU.md')
  z.write(R/'SAVING_RU.md','SAVING_RU.md')
  if PHONE:z.write(R/'PHONE_INPUT_RU.md','PHONE_INPUT_RU.md')
  if PHONE:z.write(R/'SAMPLE_NOTE_RU.md','SAMPLE_NOTE_RU.md')
  if PHONE:z.write(R/'MUSIC_LIGHTS_RU.md','MUSIC_LIGHTS_RU.md')
  if PHONE:z.write(R/'R15A_RU.md','START_RU.md')
  if PHONE:z.write(R/'HEADER_TABS_RU.md','HEADER_TABS_RU.md')
  if PHONE:z.write(R/'PAD_STEPS_RU.md','PAD_STEPS_RU.md')
  if PHONE:z.write(R/'WORKFLOWS_RU.md','WORKFLOWS_RU.md')
  if PHONE:z.write(R/'R14F_RU.md','R14F_RU.md')
  if PHONE:z.write(R/'R15_RU.md','R15_RU.md')
  if PHONE:z.write(R/'R15A_RU.md','R15A_RU.md')
  if PHONE:z.write(R/'FEATURES_RU.md','FEATURES_RU.md')
  vendor=W/'sd-clone-trial/vendor/xdg-dbus-proxy'
  for p in sorted(vendor.rglob('*')):
   if p.is_file() and 'arm64' not in p.parts and 'amd64' not in p.parts:z.write(p,'licenses/xdg-dbus-proxy/'+p.relative_to(vendor).as_posix())
 info={'revision':'r9h-save-path-diagnostics','hardware_tested':False,'stock_sha256':r3['stock_sha256'],'patched_copy_sha256':digest(patched),'zip':archive.name,'zip_sha256':digest(archive.read_bytes()),'symbols':{k:hex(v) for k,v in syms.items()},'patches':[{'file':n,'offset':hex(o),'size':len(p),'sha256':digest(p)} for n,o,p in chunks],'payload_sha256':{n:digest((out/n).read_bytes()) for n in payload_names},'session_limit':None,'saving':{'status':'experimental_not_device_verified','persistent_write_scope':['<selected SD>/MPC-Sample/Projects','<selected SD>/MPC-Sample/Samples'],'working_data':'OverlayFS_readonly_original_tmpfs_upper','ram_changes_limit_bytes':536870912,'native_working_reserve_bytes':419430400,'nominal_headroom_before_changes_bytes':117440512,'full_data_copy':False,'startup_recovery_deadline_seconds':60,'internal_save':'temporary_RAM_only','flush':'syncfs_after_clone_stopped','dbus_policy':'unchanged_no_unmount_no_mass_storage'},'value_display':'persistent_name_and_value','recent_edit_ms':None,'focus_colour_policy':'selected_amber_including_edit','discrete_scale':'native_mode_3_inclusive_range_single_active_segment','native_value_change_timer':True,'runtime_tested':False,'screen_addressing':'entry_relative_signed64_linker_deltas','background':'opaque_black','focus':'2px_top_bracket_32px_sides_name_and_value_colour','alignment':'centered_name_and_centered_text_unit_bundle','loop_lock_alignment':'right_edge_4px_reserved_numeric_space','shift_badge':'white_S_available_red_S_native_index_1','global_shift_arrow':'hidden_by_exact_component_name','shift_pair_count':19,'component_types':['ui::Slider','ui::ValueLabel'],'footer_bounds':{'slider':{'width':106,'height':38},'label':{'width':106,'height':38,'y':198,'x':[0,106,213]}},'screen_ancestors':[x['name'] for x in audit['screens']]}
 info['startup_logging']={'entry':'MPC_MOD_ENTRY.txt','events':'MPC_MOD_EVENTS.txt','console':'MPC_MOD_CONSOLE.txt','full_report':'MPC_CLONE_TRIAL.txt','controller_file':'mod-launcher.sh','controller_sha256':digest(text.encode()),'flush':'entry_sync_then_pinned_SD_syncfs_checkpoints','periodic_logging':False,'proven_after_power_loss':False}
 info['save_io_diagnostics']={'status':'experimental_not_device_verified','method':'libc interposition in clone only','library':'save-io.so','library_sha256':digest(trace),'output':'RAM app/save-io.txt included in full report','max_records':1024,'background_thread':False,'write_scope_changed':False,'scope':'open/fopen/mkdir/rename/unlink and access failures; first16 relevant statfs calls; direct syscalls or unwrapped APIs may not be captured','performance_verified':False}
 info['resource_snapshots']={'output':'resources.txt in full SD report','stages':['preparation','layer_ready','clone_verified','clone_tick20','clone_tick40','before_clone_stop'],'during_active_app':'RAM_only_at_two_ticks','periodic_observer_process':False,'memory_max_unchanged_bytes':838860800}
 if PHONE:
  info['revision']=PHONE_REV
  info['event_audition']={'revision':'r16d','status':'Chop_direct_Sample_receiver_trial_L_hardware_confirmed','native_input':'L_1e27360_C_211b090_to_211aa30','native_release':'1e270d0_source1_original_pad_index','staged_override':'thread_local_exact_properties_pointer_in_existing_SIXTEEN_PROCESS_hook','queue':'native_Copy_context_worker_owned_fixed64_slots','no_EventAudition':True,'source_mapping':'MIDI_to_pad_Program14a8_then_native2115070_maps_pad_to_MIDI12a8','original_sequence_suppression':'bounded_lockfree_snapshot_match_at_sequence_routes_only_not_pad_input','chop_skips_live_MIDI_mapping':True,'levels_hardware_verified':True,'storage_modified_by_preview':False,'record_mode':'None_required_on_UI_and_worker','original_colour':'amber','choice_colour':'cyan','scoped_callback_watchdog_ticks':15,'hardware_verified':False}
  info.pop('save_io_diagnostics',None)
  info['phone_input']={'status':'built_not_device_verified','library':library,'library_sha256':digest(trace),'layout':'PHONE_INPUT_RU.md','simultaneous_native_input':['encoder','plus','minus'],'multitap_ms':1000,'uppercase':'hold_pad12','numeric_toggle':'pad4','symbols':'pad8: .-_#','delete':'pad16','cursor':'pad1_pad3','native_ui_timer_ms':50,'background_observer_thread':False,'screens':['ProjectSaveScreen','SampleSaveScreen'],'thread_guard':'hardware_producer_to_UI_Timer_FIFO_no_component_access_from_producer','queue_capacity':128,'queue_payload':'numeric_pad_data_and_copied_native_control_functors_no_component_pointers','queue_generation':'stale_keyboard_events_discarded_native_controls_delivered_once','rgb':'shared_native_23b0ec0_sender_override_and_restore','native_cursor':'commit_multi_tap_without_extra_cursor_step_before_stock_action'}
  info['phone_input'].update(base_keyboard_user_confirmed='r10a',letter_pad13='_',numeric_pad13='1',cursor_rgb='2030ff',cursor_brightness=0.40,delete_rgb='ff0000',delete_brightness=0.65,pressed_brightness=0.85,delete_pressed_brightness=1.0,press_feedback_minimum_ms=100,feedback_restore='base_palette_after_release_and_minimum_flash; held_keys_stay_bright',feedback_midi_update='only_changed_pads; full_palette_on_mode_or_case_change')
  info['sample_note']={'status':'r11_UI_user_confirmed_r12_persistence_not_device_verified','screen':'Sample Tune','original_note':'Shift+K1; blank_or_pitch_class_C_to_B','semi_display':'native_semitones_and_computed_pitch_class','native_pitch_modified_by_original_note':False,'storage':'optional_named_native_project_JSON_extension','sample_identity':'native_sample_pool_name_within_current_project','project_serialization':True,'rename_migration':False,'standalone_sample_metadata':False,'ui_bridge':'guarded_footer_callback_SampleModeScreen_plus_60b8','input':'existing_UI_FIFO_and_50ms_native_Timer','extra_observer_thread':False}
  info['music_lights']={'status':'built_not_device_verified','project_menu':'Key_Scale_row4_then_native_white_Mod_Off_row5_with_confirmation','project_key_saved_in_native_key':True,'sample_pages':['Tune','Play','Lights'],'normal_knobs':['Mode','Tonic_Project_or_local','Scale_Project_or_local'],'shift_knobs':['Transpose16','Role','Colour'],'modes':['Monochrome','Piano','Tonic','Scale_3_roles'],'scales':['Major','Natural Minor','Dorian','Phrygian','Lydian','Mixolydian','Locrian','Chromatic'],'transpose_range':[-24,24],'transpose_scope':'native_16_Levels_Tune_event_processor_only','lighting':'native_SixteenLevelLighting_layer_refresh_no_UI_LED_manager_calls','refresh':'next_native_refresh_reenter_16_Levels_after_setting_changes','unknown_root':'stock_lights','persistence_field':'data.mpcSampleModV1','persistence_format':'MPCS12','stock_resave_preserves_extension':False,'new_project':'reset_only_confirmed_New_Project','load':'commit_metadata_only_after_native_JSON_reader_success','capacity':512}
  info['music_lights'].update(refresh='on_atomic_music_snapshot_change_via_native_lighting_queue',refresh_queue_va='0x2b59300',refresh_callback_va='0x1538cd0',refresh_payload='exact_native_constructor_functor_153ad90_153adf4',refresh_lifetime='native_layer_plus_48_reference_and_ctor_dtor_guard',refresh_layer_capacity=8,refresh_poll_ui_ms=50,refresh_no_new_thread=True,encoder_button_code='0x6f',encoder_focus='click_forward_Shift_click_reverse_skip_disabled_fields',parameter_scales='discrete_segments_current_only_Transpose16_zero_marker',fix_status='built_not_device_verified')
  info['header_buttons']={'status':'built_not_device_verified','renderer':'shared_native_top_button_class_706e940_paintOverChildren_slot','screen_ancestors':[x['name'] for x in audit['screens']],'bounds':'top45px_width50_to107_height18_to45','modes':['action','toggle','tabs'],'background':'black','alignment':'centered_label','selected_tabs':'amber_label_and_segment','inactive_tabs':'white_label_grey_segments','action_colour':'native_TextButton_textColourOffId','segment_px':{'width':5,'gap':2,'active_height':3,'inactive_height':1},'sample_tune_count':3,'inactive_sample_tune':'three_segments_also_on_Trim_or_Filter','hit_areas_changed':False,'native_page_count_mutated':False,'input_bindings_changed':False,'runtime_audit':'once_per_audited_screen_and_header_column_RAM_only'}
  info['pad_steps']={'status':'built_not_device_verified','entry':'Shift+SEQ > B2','view':'native_Step_Edit_screen_overlay','events':'native_tag3_note_variant_only','add':'native_19500e0_empty_removals_one_added_note','delete':'native_194fa30','change':'native_194fbb0','raw_event_vector_writes':False,'factory_hook':False,'clock':'native_2113b10_transport_getter_from_NowPosition_258','transport_state':'isPlaying_virtual_18_on_engine_768_facade_148','step_write':'local_SEQ_RECORD_toggle_default_off_stop_or_play','record_led':'native_23b0de0_CC_50_override_and_restore','bank_selection':'native_bank_browses_only_B1_pad_or_K1_commits_sound','bank_sync':'native_PadBankControls_ctor_dtor_owned_list_plus_140_selected_158','sample_lookup':'program_instrument_by_pad_index_not_MIDI_note','audio_clock_synthesized':False,'pad_ticket':'atomic_numeric_epoch_page_sound_grid_shift_snapshot','colour_roles':{'green':'note','amber':'edit_selection','red':'native_playhead'},'knobs':['Sound','Grid','Velocity'],'encoder':'Steps_cursor_only','velocity_scale':'continuous_1_to_127','sound_select':'hold_B1_plus_pad','shift_pad':'native_global_shortcuts_suspend_and_resume_Steps','delete_gesture':'hold_Erase_plus_pad','pad_order':'13_14_15_16_9_10_11_12_5_6_7_8_1_2_3_4','lcd_grid':False,'empty_sample':'Empty','exit_steps':'B2_only_except_project_reset_or_fault','shift_k1':'Bank_A_to_H','grid':['1/4','1/8','1/16','1/32','1/64'],'shift_k2':'Triplet_Off_On','copy_collision':'multiple_events_allowed_identical_single_event_skipped','complex_events':'native_Events_only','events_encoder':'circular_native_Q_cursor','events_shift_encoder':'move_selected_note_by_native_Q_wrap_note_and_view','events_selection':'native_full_event_equality_including_pitch_and_duration','hardware_verified':False}
 if PHONE:
  info['pad_steps'].update({'delete':'native_194fa30_single_or_19500e0_group_removal_multiset',
   'colour_roles':{'green':'events','amber':'selection','cyan':'playhead_and_event_playback','red':'erase','violet':'muted_events'},
   'events_erase':'tap_release_selected_row_or_native_Erase_pad_confirmation_entire_sequence',
   'events_B3':'free_in_main_Events_Apply_in_picker',
   'event_picker':'context_C_Chop_L_Levels_CL_both_in_Events_and_Steps_B3_Apply_B1_cancel',
   'event_picker_modes':['Velocity','Tune','Filter','Layer','Attack','Decay','Probability','Ratchet','Slice','Articulation'],
   'event_picker_processor':'unhooked_native_21472f0_on_stack_properties_and_copied_event',
   'event_picker_origin':'infer_active_modifier_and_nondefault_data_no_persistent_input_mode_tag',
   'events_shift_K3':'direct_native_16_candidate_choice',
   'playback_lights':'UI_native_clock_120ms_outer_Sample_sources_inner_10_Levels_modes_and_Chop',
   'playback_context':'SampleScreen148_CurrentThings3d0_clip688_list_668_locked_Levels_instrument',
   'native_button_lights':'CC_28_4b_available_event_context_Save24_untouched_SEQ_unchanged',
   'mute':'native194fd60_replacement_multimap_one_Undo_duplicates_preserved',
   'mute_button':'CC_2b_GlobalControls_25c0_native_MutePadMode_30f4',
   'sound_palette':'native_2804760_has_sample_all_layers_in_browsed_bank',
   'event_picker_lcd':'grouped_identity_position_before_after_text_K1_type_K2_choice_K3_range',
   'steps_multi_event_picker':'filter_selected_sound_step_C_or_L_one_direct_multiple_choose_list_max64_stale_revalidate',
   'event_markers':'dedicated_native_row_pad_label_suffix_C_slice_modifier_L_persisted_levels_modifiers_no_provenance_for_plain_velocity',
   'chop_availability':'C_marker_only_event_Slice_modifier',
   'chop_button':'CC4b_GlobalControls2510_native1541bc0_Slice8',
   'chop_state_hook':'ChopPadsLighting152a870_properties_f8',
   'steps_layout':'black_sound_identity_navigation_transport_status_groups',
   'event_label_callsite':'16a8418_x24_event_x8_String'})
  info['header_buttons']['confirmation_captions']='native_QuestionScreen_no_overlay'
  info['music_lights_inheritance_labels']='effective_tonic_and_scale_with_P_suffix'
  info['workflows']={'status':'built_not_device_verified','batch_copy_limit':16,
   'copy_context':'base_subobject48_of_AC50CopyMode_final_vptr6e0dfd8_or_base6d8cd88',
   'copy_entry':'Sample_Shift_Erase_then_B2_Batch','dispatcher':'context68_CommandManager_direct_no_offset60','executor':'native_ThreadQueues_worker_no_new_thread','progress':'atomic_prepare_copy_rollback_done_until_ack','target_input':'source_pad_selects_row_other_pad_sets_target','target_K2':'Auto_then_pad1_to16_in_browsed_bank','pad_colours':{'source':'green','free':'violet','occupied':'red','cursor':'amber','assigned_target':'cyan'},'bank_browse':'preserve_assignments_K1_sets_auto_bank','occupied_target':'exclamation_and_red_pad_two_B3_confirmation',
   'done_ack':'native_ThreadQueues_worker_mode30ed_exit_then_release_UI_pad_capture','cancel':'same_worker_exit_from_Sources_or_Targets_no_copy','shift_release':'forwarded_during_copy_and_exit','auto_K3':['First_free_previous_across_banks','Same_pad','Bank_free_current_auto_bank_only'],'entry_gesture':'native_Shift_Erase_release_on_B2_paired_suppression_then_keep_Copy_modal_host_by_two_exact_release_callsites','release_guard_callsites':['0x14c0f84','0x14c0fd4'],'encoder_press':'audition_selected_source_on_native_worker_not_copy_confirmation','audition_recording':'native_record_None_guard_UI_and_worker_no_arm_changes','plus_minus':'Targets_rows_Sources_banks_plus36_minus37','missing_targets':'persistent_red_warning_and_No_space_caption','rgb_refresh':'workflow_priority_and_generation_reassert_after_bank_change','browser_search':{'entry':'Shift_B3','scopes':['Folder','Subfolders','Library'],'scope_K3':'query_only','default_scope':'Library','library_root':'containing_Samples_or_Projects_otherwise_current_folder','sample_load':'native14fc6a0_doubleclick1_after_silent_selected_pad28f9d60','project_load':'native14fb1c0_mode0','result_pads':'silent_destination_only_B3_load','result_banks':'native_bank_button_next_Shift_previous','limits':{'files':4096,'directories':512,'depth':16,'entries_visited':50000},'symlinks':'skip','background_thread':False},'chop_export_enabled':True,'cold_boot_with_autorun':'unresolved_hang_before_prompt_hot_insertion_only'}
 if PHONE:
  info['pad_steps'].update({'mute_playback_gate':'213ba04_branch_tag3_bit9_note_on_only_NoteOff_bit8_passes_no_callback',
   'shift_encoder':'selected_sound_step_group_move_grid_wrap_preserve_offsets_one_Undo',
   'shift_mute':'unmute_all_events_of_selected_sound_whole_sequence_one_Undo',
   'shift_rgb':'retain_Steps_palette_native_Shift_pad_shortcuts_preserved',
   'event_picker_audition':'native_EventAudition_ctor18b04f0_start18b0510_stop18b0c00_note_copy_no_apply_until_B3',
   'context_buttons':'selected_cursor_page_independent_of_playback_Follow_page',
   'muted_selection':'amber_edit_cursor_cyan_playhead_muted_LCD_violet',
   'knob_fx_theme':'opaque_full_width_amber_underlay_black_footer_and_S_badge_hides_parent_focus',
   'mute_playback_gate_r15':['211a234_sequence_input_x4','2119d04_sampler_route_x3','213ba04_output_route_x3']})
 if PHONE:info['workflows']['chop_export']={'status':'experimental_device_trial','entry':'Chop_Shift_B2','buttons':['B1_native_Extract','B2_Batch','B3_native_Split'],'limit':16,'audio':'native_ExtractAndAssign_independent_cropped_Sample','sequence':'Copy_then_Extract_each_row_one_group_Undo','output_validation':'created_sample_assigned_to_target_and_frame_count_matches','source_protected':True,'maximum_new_PCM_bytes':67108864,'persistence_hardware_tested':False}
 if PHONE:
  info['workflows']['chop_export'].update({'host':'ordinary_SampleModeScreen_Chop_Split_visible_a10','global_controls_offset':'0x108','current_things_offset':'0x148','independent_of_SixteenLevels_Slice':True})
  info['workflows']['browser_search'].update({'current_directory':'native_BrowserLogic1db08c0_x8_File_not_model38_cache','symlinks':'read_only_follow_device_inode_dedup_512','hidden_folders':True,'root_failure':'visible_error_no_silent_folder_fallback','preview':'B3_native14fb1c0_onSamplePreview_service90','sample_load':'encoder_native14fc5a0_onSampleSelected_service10_after_silent_selected_pad28f9d60_keep_search','project_load':'encoder_confirm_then_B3_native14fc6a0_onProjectSelected_service30_bool1','context_types':{'samples':0,'projects':1},'scope_K3':'Shift_K3_in_sample_query','locate':'Shift_encoder_native_directory14fc7e0_then_async_exact_File_row_select3b71f20_no_load','preview_volume':'K3_Sample_Select_and_search_native_SampleFileAudition_c0_value110_0to1_Property_notify20bc210_preview_instrument20_only'})
  info['pad_steps']['event_picker_audition_context']='native_sequencer_CurrentThings_distinct_from_UI_CurrentThings_no_pointer_equality'
  info['pad_steps']['event_picker_audition_fallback']='session_lifetime_native_ctor_existing_ThreadQueues_copy48_CurrentThings_sample148_Sequencer_sample140_receiver5c0'
  info['workflows']['browser_search']['limits']['files']=4096
  info['workflows']['browser_search']['limit_semantics']='stored_matching_results_not_first_files_query_restarts_partial_or_truncated_scan'
  info['workflows']['browser_search']['result_pads']='silent_destination_encoder_load_keep_search'
  info['workflows']['browser_search'].update({'preview_volume':'native_callback1500620_1500560_inline_SampleFileAudition90_no_Copy_dependency','preview_default_percent':25,'preview_gain':'native_Main_global_input_bypass_custom_UI_plus_independent_K3_pre_master_attenuation_no_double_Main_gain','main_volume_binding':'1ba88b0_parameter4_curve1_raw_AC50ControlSurfaces149d100_plus2ea8','preview_fail_closed':'block14fb1c0_without_verified_master_audition_and_instrument20_curve0_1_2_3','preview_live_update':'existing_UI_timer_no_new_thread'})
  info['workflows']['browser_search'].update({'preview_volume_visibility':'selected_existing_audio_file_only_native_optional_selection_or_search_result','delete':'Erase_exact_native_B3_Delete16056c0_14fc8f0_User_audio_native_confirmation_search_locate_first','shift_B2_eject':'hidden_and_consumed_in_mod_browser'})
  info['workflows']['chop_export'].update({'audition':'disabled_after_r15c_device_crash_no_EventAudition_call_from_Batch','device_r15b':'three_slices_extracted_successfully_targets_B01_B02_B03_user_confirmed', 'context_validation':'same_Program_not_CurrentThings_pointer_identity','source':'Sample148_CurrentThings668_locked_instrument_fallback60','entry_gate':'visible_ordinary_Chop_Split_allows_nonSample_mode_id','diagnostics':'BATCH_OPEN_or_explicit_REJECT_reason'})
  info['pad_steps']['event_picker_audition_fallback']='native_UI_registry_ThreadQueues_key71657a0_without_Copy_dependency'

 if PHONE:
  info['workflows']['batch_events']={'control':'Targets_Shift_K3','default':'Sound_only','scope':'each_existing_sequence_current_kit_track_independently','preserve':'source_and_target_existing_events_full_inline_C_L_mute_payload','undo':'one_outer_group_for_sounds_and_events','snapshot':'all_sequences_before_first_sound_write','max_new_events':32768,'hardware_tested':False}
  info['workflows']['browser_search']['preview_mode']='B3_toggle_autoplay_selected_result_stop_on_navigation'
  info['workflows']['batch_audition_loop']='one_shot_not_yet_fixed_source_loop_preserved_Chop_audition_disabled'
 if PHONE:
  info['music_lights'].update(modes=['Monochrome','Piano','Tonic','Scale','Degrees'],persistence_format='MPCS13_read_MPCS12',original_pitch_colour='Magenta_default_Shift_Role_Original_Colour')
  info['pad_steps']['colour_roles']={'note':'green','mute_mode_active_note':'yellow','muted':'red','erase_available':'orange','selection':'white_pulse_alternates_semantic_colour','playhead':'cyan'}
  info['workflows']['chop_export']['audition']='private_SampleFileAudition20be470_range_in_owned_worker_payload_click_toggle_no_EventAudition'
  info['workflows']['browser_search']['stop']='disable_queued_private_play_plus_native20beec0'
  info['workflows']['batch_audition_loop']='Chop_private_preview_one_shot_source_loop_preserved_Copy_native_audition_unchanged'
  info['pad_steps']['mute_gesture']='tap_toggle_hold_off_on_release_including_previously_latched_mode'
  info['naming_shift']='both_edges_preserve_pad_text_case_cursor_rebuild_glyphs_skip_underlying_UI'
 if PHONE:
  info['workflows']['batch_audition_loop']='Copy_and_Chop_private_sample_one_shot_current_trim_no_source_loop_change'
  info['workflows']['batch_preview_completion']='native_BoolProperty298_worker_start_guard_natural_end_resets_toggle'
  info['workflows']['transport']='Play52_Stop51_passthrough_in_Search_Batch_and_progress_Stop_also_stops_preview'
  info['project_exit']='last_row_native_white_no_menu_overlay_dispatch_to_tagged_UI_confirmation_B1_cancel_B3_confirm_RAM_request'
  info['browser_storage_caption']='native_current_directory_media_SD_and_data_Internal_RAM_no_path_retarget'
 if PHONE:
  info['workflow_layout']={'revision':'r16','scope':['Steps','BatchSources','BatchTargets','SearchQuery','SearchResults'],'lcd':[320,240],'mapping_rows':3,'main_font_px':[18,25],'support_font_px':[12,15],'alignment':'left_names_aligned_table_columns_right_status','long_names':'UTF8_safe_visual_ellipsis_file_names_unchanged','input_audio_and_storage_logic':'unchanged'}
 (R/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
 print(json.dumps(info,indent=2))
if __name__=='__main__':main()
