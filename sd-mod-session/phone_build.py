"""Generate pinned ARM64 bridges for the experimental phone-input build.

The first PACIASP/BTI instruction remains at each native entry. The bridge
undoes that entry PAC before tail-calling the C callback; the separate original
trampoline signs its own LR and executes the displaced non-PC-relative word.
Callbacks are installed into a dedicated table only in the clone's memory.
"""
from pathlib import Path
import json
import struct
import subprocess

R = Path(__file__).resolve().parent
CODE_VA = 0x6D3D000
TABLE_VA = 0x6D42000
BRIDGE_MAGIC = b'MPCPHONE-R13-V1'
BRIDGE_STRIDE = 96
HOOKS = [
    ('REBUILD', 0x15BDB10), ('DTOR', 0x15BEC60), ('DELETE', 0x15BEDF0),
    ('PAD', 0x159ED30), ('CONTROLLER', 0x159F220), ('BUTTONS', 0x159F2F0),
    ('SAMPLE_PAINT', 0x1685E50), ('PROJECT_PAINT', 0x1649D30), ('RGB', 0x23B0EC0),
    ('CHAR_INCREMENT', 0x15BE020), ('CASE', 0x15BE060), ('MOVE', 0x15BE070),
    ('CLEAR', 0x15BE100), ('ERASE', 0x15BE4B0), ('PLUS', 0x15BE5E0),
    ('MINUS', 0x15BE740), ('SCROLL', 0x15BE8B0), ('RIGHT', 0x15BEA50),
    ('SAMPLE_DTOR', 0x1678760),
    ('PROJECT_DTOR', 0x16456D0), ('SIXTEEN_REFRESH', 0x1545930),
    ('SIXTEEN_PROCESS', 0x21472F0), ('MUSIC_SAVE', 0x1D409A0),
    ('MUSIC_LOAD', 0x148D830), ('MUSIC_READ', 0x1490650),
    ('MUSIC_KEY', 0x1D25520), ('NEW_PROJECT', 0x1642A00),
    ('SIXTEEN_CTOR', 0x1545A50), ('SIXTEEN_DTOR', 0x1547B70),
    ('STEP_DTOR',0x16AEA50), ('NOW_TICK',0x2109A90),
    ('BANK_CTOR',0x14D7D50), ('BANK_DTOR',0x14D7460),
    ('BUTTON_LIGHT',0x23B0DE0),
    ('CHOP_REFRESH',0x152A870),
    ('FILES_DTOR',0x160CAE0),('COPY_DTOR',0x15FA930),
    ('COPY_CONTEXT_CTOR',0x1518980),('COPY_CONTEXT_DTOR',0x1517C70),
    ('AUDITION_CTOR',0x18B04F0),
    ('FILES_CTOR',0x160BBF0),
    ('MASTER_VOLUME_CTOR',0x1BA88B0),('FILE_PREVIEW',0x14FB1C0),
    ('PRIVATE_PREVIEW_WORKER',0x20BD5D0),
]
FOOTER_VA = CODE_VA + len(HOOKS) * BRIDGE_STRIDE
PROJECT_REQUEST_VA = FOOTER_VA + BRIDGE_STRIDE


def build(stock, run, branch):
    audit=json.loads((R/'ui-audit.json').read_text())
    screens=[int(x['vptr']) for x in audit['screens']]
    (R/'build/tab_screens.h').write_text('/* Pinned existing UI audit. */\n'
        '#include <stdint.h>\n#define TAB_SCREEN_COUNT '+str(len(screens))+'\n'
        'static const uintptr_t tab_screens[TAB_SCREEN_COUNT]={'+','.join(hex(x) for x in screens)+'};\n')
    assembly = ['.arch armv8.3-a', '.text']
    records = []
    for i, (name, entry) in enumerate(HOOKS + [('NOTE_FOOTER', 0), ('MUSIC_PROJECT', 0), ('EVENT_LABEL',0),('COPY_SOURCE',0),('COPY_RELEASE_EXIT',0),('PREVIEW_EVENT_GATE',0),('SAMPLE_FILE_COPY',0xf5cc80)]):
        if not entry:
            label='phone_'+name.lower()
            slot=TABLE_VA+64+i*16+8
            assembly += [f'.org {i*BRIDGE_STRIDE}',f'.global {label}',label+':','bti c',
                         f'adrp x16, phone_slot_{i}',f'ldr x16,[x16,:lo12:phone_slot_{i}]',
                         f'cbz x16, {label}_empty']
            if name=='EVENT_LABEL':
                # Only 16a8418: x24 is the current 56-byte event; x8 owns the
                # native nontrivial String result. Preserve pad index in w0.
                assembly += ['mov x1,x24','mov x2,x8','br x16',label+'_empty:','b phone_native_pad_label']
            elif name=='COPY_SOURCE':
                assembly += ['br x16',label+'_empty:', 'b phone_native_copy_source']
            elif name=='COPY_RELEASE_EXIT':
                assembly += ['br x16',label+'_empty:', 'b phone_native_mode_exit']
            else:
                assembly += ['br x16',label+'_empty:', 'mov w0,#0','ret']
            records.append({'name':name,'entry':0,'pac':False,'slot':slot,'trampoline':0})
            continue
        first, displaced = struct.unpack_from('<II', stock, entry)
        assert first in (0xD503233F, 0xD503245F), (name, 'PACIASP or BTI c required')
        # None of these displaced words is ADR/ADRP, a branch, or a literal load.
        assert displaced & 0x1F000000 not in (0x10000000, 0x14000000, 0x18000000)
        assert displaced & 0xFF000010 not in (0x54000000,)
        assert displaced & 0x7E000000 not in (0x34000000, 0x36000000)
        pac = first == 0xD503233F
        label = 'phone_' + name.lower()
        assembly += [f'.org {i * BRIDGE_STRIDE}', f'.global {label}', label + ':', 'bti c']
        if pac:
            assembly += ['autiasp']
        slot = TABLE_VA + 64 + i * 16 + 8
        assembly += [f'adrp x16, phone_slot_{i}', f'ldr x16,[x16,:lo12:phone_slot_{i}]',
                     f'cbz x16, {label}_original', 'br x16',
                     f'.global {label}_original', label + '_original:', 'bti c']
        if pac:
            assembly += ['paciasp']
        assembly += [f'.inst {displaced:#x}', f'b phone_continue_{i}']
        records.append({'name': name, 'entry': entry, 'pac': pac,
                        'displaced': f'{displaced:08x}', 'slot': slot})
    # Native213ba00 dispatches note events without checking IsMuted(bit9).
    # Filter only muted note-ons; generated note-offs(bit8) always pass.
    # No callbacks, allocations, locks or model traversal on the audio path.
    assembly += [f'.org {len(records)*BRIDGE_STRIDE}', '.global phone_muted_note_gate',
        'phone_muted_note_gate:', 'bti c', 'ldr w9,[x3,#8]', 'and w10,w9,#255',
        'cmp w10,#3','b.ne phone_note_dispatch_original','tbnz w9,#8,phone_note_dispatch_original',
        'tbz w9,#9,phone_note_dispatch_original','autiasp','ret',
        'phone_note_dispatch_original:','sub sp,sp,#0x140','b phone_note_dispatch_continue']
    # Sequence playback has a separate sampler route. Filter before its
    # event expansion (ratchet/probability) and at the sequence input boundary.
    for label,entry,register in [('sequence',0x2119d00,'x3'),('sequence_input',0x211a230,'x4')]:
        original=struct.unpack_from('<I',stock,entry+4)[0]
        assembly += ['.global phone_muted_'+label+'_gate','phone_muted_'+label+'_gate:','bti c',
            f'ldr w9,[{register},#8]','and w10,w9,#255','cmp w10,#3',f'b.ne phone_{label}_original',
            f'tbnz w9,#8,phone_{label}_original',f'tbz w9,#9,phone_{label}_preview','autiasp','ret',
            f'phone_{label}_preview:','sub sp,sp,#96',
            'stp x0,x1,[sp]','stp x2,x3,[sp,#16]','stp x4,x5,[sp,#32]','stp x6,x7,[sp,#48]','stp x8,x30,[sp,#64]',
            'mov x0,'+('x2' if label=='sequence' else 'x3'),'mov x1,'+register,
            'mov w2,w5' if label=='sequence' else 'mov w2,#0x7ffffffe',
            'bl phone_preview_event_gate','mov w9,w0',
            'ldp x0,x1,[sp]','ldp x2,x3,[sp,#16]','ldp x4,x5,[sp,#32]','ldp x6,x7,[sp,#48]','ldp x8,x30,[sp,#64]',
            'add sp,sp,#96',f'cbz w9,phone_{label}_original','autiasp','ret',
            f'phone_{label}_original:',f'.inst {original:#x}',f'b phone_{label}_continue']
    (R / 'build/phone_bridges.S').write_text('\n'.join(assembly) + '\n')
    (R / 'build/phone_bridges.ld').write_text(
        f'SECTIONS {{ . = {CODE_VA:#x}; .text : {{ *(.text) }} '
        '/DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) } }\n' +
        '\n'.join(f'phone_slot_{i} = {r["slot"]:#x}; phone_continue_{i} = {r["entry"] + 8:#x};'
                  for i,r in enumerate(records)) + '\nphone_native_pad_label = 0x2d444e0;\nphone_native_copy_source = 0x286bbe0;\nphone_native_mode_exit = 0x1c0d990;\nphone_note_dispatch_continue = 0x213ba08;\nphone_sequence_continue = 0x2119d08;\nphone_sequence_input_continue = 0x211a238;\n')
    run('aarch64-linux-gnu-gcc', '-c', 'build/phone_bridges.S', '-o', 'build/phone_bridges.o')
    run('aarch64-linux-gnu-ld', '-T', 'build/phone_bridges.ld',
        'build/phone_bridges.o', '-o', 'build/phone_bridges.elf')
    run('aarch64-linux-gnu-objcopy', '-O', 'binary', '--only-section=.text',
        'build/phone_bridges.elf', 'build/phone_bridges.bin')
    nm = subprocess.check_output(['aarch64-linux-gnu-nm', str(R / 'build/phone_bridges.elf')], text=True)
    symbols = {line.split()[2]: int(line.split()[0], 16) for line in nm.splitlines()}
    for r in records:
        if r['entry']:r['trampoline'] = symbols['phone_' + r['name'].lower() + '_original']
    assert symbols['phone_note_footer']==FOOTER_VA
    assert symbols['phone_music_project']==PROJECT_REQUEST_VA
    header = ['/* Generated from pinned 1.3.0.12 ELF; do not edit. */',
              '#include <stdint.h>', f'#define PHONE_TABLE_VA {TABLE_VA:#x}',
              '#define PHONE_BRIDGE_MAGIC "'+BRIDGE_MAGIC.decode()+'"', 'enum {']
    header += ['H_' + r['name'] + ',' for r in records]
    header += ['H_COUNT };', 'struct phone_hook { uintptr_t entry,trampoline; };',
               'static const struct phone_hook phone_hooks[H_COUNT] = {']
    header += ['{%#x,%#x},' % (r['entry'], r['trampoline']) for r in records]
    header += ['};']
    (R / 'build/phone_hooks.h').write_text('\n'.join(header) + '\n')
    run('aarch64-linux-gnu-g++','-c','-fPIC','-O2','-std=c++11','-Wall','-Wextra','-Werror',
        '-mbranch-protection=standard','src/step_exceptions.cc','-o','build/step_exceptions.o')
    run('aarch64-linux-gnu-g++','-c','-fPIC','-O2','-std=c++11','-Wall','-Wextra','-Werror',
        '-mbranch-protection=standard','src/step_mute.cc','-o','build/step_mute.o')
    run('aarch64-linux-gnu-g++','-c','-fPIC','-O2','-std=c++11','-Wall','-Wextra','-Werror',
        '-mbranch-protection=standard','src/batch_copy.cc','-o','build/batch_copy.o')
    run('aarch64-linux-gnu-g++','-c','-fPIC','-O2','-std=c++11','-Wall','-Wextra','-Werror',
        '-mbranch-protection=standard','src/batch_events.cc','-o','build/batch_events.o')
    run('aarch64-linux-gnu-gcc', '-shared', '-fPIC', '-O2', '-std=c11',
        '-Wall', '-Wextra', '-Werror', '-Wcast-function-type', '-Ibuild','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
        '-mbranch-protection=standard', '-Wl,-z,noexecstack,-z,relro,-z,now',
        'src/phone_input.c', 'src/phone_events.c', 'src/phone_native.c', 'src/sample_load_trace.c',
        'src/note_native.c', 'src/note_abi.S', 'src/music_settings.c',
        'src/music_archive.c', 'src/music_ui.c', 'src/music_lighting.c',
        'src/step_native.c','src/step_abi.S','src/workflow_ui.c','src/workflow_abi.S','src/browser_tree.c',
        'build/step_exceptions.o','build/step_mute.o','build/batch_copy.o','build/batch_events.o','-lstdc++','-ldl', '-pthread', '-lm', '-o', 'build/phone-input.so')
    code = (R / 'build/phone_bridges.bin').read_bytes()
    assert len(code) < TABLE_VA - CODE_VA
    def signed(value, bits):
        return value - (1 << bits) if value & (1 << (bits - 1)) else value
    # Inspect emitted relocation words as part of building a pinned patch. An
    # assembler numeric operand is PC-relative; absolute firmware VAs must be
    # linker symbols, not numeric instruction operands.
    for i,r in enumerate(records):
        pc=CODE_VA+i*BRIDGE_STRIDE+4+(4 if r['pac'] else 0)
        word=struct.unpack_from('<I',code,pc-CODE_VA)[0]
        imm=((word >> 29)&3) | (((word >> 5)&0x7ffff)<<2)
        page=(pc&~4095)+(signed(imm,21)<<12)
        assert page == r['slot'] & ~4095, 'ADRP relocation must reach bridge table'
        ldr=struct.unpack_from('<I',code,pc+4-CODE_VA)[0]
        assert ((ldr>>10)&4095)*8 == r['slot'] & 4095
        if r['entry']:
            tail=r['trampoline']+8+(4 if r['pac'] else 0)
            word=struct.unpack_from('<I',code,tail-CODE_VA)[0]
            assert word & 0xfc000000 == 0x14000000
            assert tail+(signed(word&0x3ffffff,26)<<2) == r['entry']+8, 'original trampoline return'
    table = bytearray(4096)
    table[:len(BRIDGE_MAGIC)+1] = BRIDGE_MAGIC+b'\0'
    for i, r in enumerate(records):
        struct.pack_into('<QQ', table, 64 + i * 16, r['entry'], 0)
    assert len(table) == 4096
    chunks = [('phone-code.bin', CODE_VA, code), ('phone-table.bin', TABLE_VA, bytes(table))]
    assert stock[0x213ba00:0x213ba08]==bytes.fromhex('3f2303d5ff0305d1')
    chunks.append(('phone-muted-note-gate.bin',0x213ba04,branch(0x213ba04,symbols['phone_muted_note_gate'])))
    for label,entry,expected in [('sequence',0x2119d00,'3f2303d5ff8305d1'),('sequence_input',0x211a230,'3f2303d5fd7bbca9')]:
        assert stock[entry:entry+8]==bytes.fromhex(expected)
        chunks.append(('phone-muted-'+label+'-gate.bin',entry+4,branch(entry+4,symbols['phone_muted_'+label+'_gate'])))
    assert stock[0x16a8418:0x16a841c]==branch(0x16a8418,0x2d444e0,True)
    chunks.append(('phone-event-row-label.bin',0x16a8418,branch(0x16a8418,symbols['phone_event_label'],True)))
    assert stock[0x19875ac:0x19875b0]==branch(0x19875ac,0x286bbe0,True)
    chunks.append(('phone-copy-source.bin',0x19875ac,branch(0x19875ac,symbols['phone_copy_source'],True)))
    for site in [0x14c0f84,0x14c0fd4]:
        assert stock[site:site+4]==branch(site,0x1c0d990,True)
        chunks.append(('phone-copy-release-'+hex(site)[2:]+'.bin',site,branch(site,symbols['phone_copy_release_exit'],True)))
    for r in records:
        if not r['entry']:continue
        label = 'phone_' + r['name'].lower()
        chunks.append(('phone-' + r['name'].lower().replace('_', '-') + '.bin',
                       r['entry'] + 4, branch(r['entry'] + 4, symbols[label])))
    (R / 'build/phone-hook-map.json').write_text(json.dumps(records, indent=2) + '\n')
    return chunks, symbols
