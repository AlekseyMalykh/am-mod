"""Build the RAM-only named UAC2 function from the exact inMusic GPL source.

Never installs a module, changes a running gadget, or writes firmware.
Requires an extracted, prepared official kernel tree and exact firmware config.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess

ROOT=Path(__file__).resolve().parent
SOURCE_ARCHIVE_SHA256='9d6999d9f69f5afd396a634facadc9219dcb85407264273d912355dd6db38f9b'
KERNEL_RELEASE='6.12.60-imb-2025-12-03-rt13'
CHANNELS=['Master 1','Master 2']+[f'Track {track} {3+2*i+side}' for i,track in enumerate('ABCDEFGH') for side in range(2)]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()

def patched_source(source):
    def replace(old,new):
        nonlocal source
        assert source.count(old)==1,(old,source.count(old))
        source=source.replace(old,new)
    replace('static struct usb_string strings_fn[NUM_STR_DESCRIPTORS + 1] = {};',
        'enum { AM_FIRST_CHANNEL = NUM_STR_DESCRIPTORS, AM_PC_RETURN = AM_FIRST_CHANNEL + 18, AM_STRING_COUNT = AM_PC_RETURN + 2 };\n'
        'static struct usb_string strings_fn[AM_STRING_COUNT + 1] = {};\n'
        'static const char *const am_channel_names[18] = '+json.dumps(CHANNELS).replace('[','{').replace(']','}')+';')
    replace('\tus = usb_gstrings_attach(cdev, fn_strings, ARRAY_SIZE(strings_fn));',
        '\t/* This RAM function is only for the Am MOD 18-channel profile. */\n'
        '\tif (num_channels(uac2_opts->p_chmask) != 18 || num_channels(uac2_opts->c_chmask) != 2)\n'
        '\t\treturn -EINVAL;\n'
        '\tfor (unsigned int i = 0; i < 18; ++i)\n'
        '\t\tstrings_fn[AM_FIRST_CHANNEL + i].s = am_channel_names[i];\n'
        '\tstrings_fn[AM_PC_RETURN].s = "USB Return 1";\n'
        '\tstrings_fn[AM_PC_RETURN + 1].s = "USB Return 2";\n'
        '\tus = usb_gstrings_attach(cdev, fn_strings, AM_STRING_COUNT);')
    replace('\tif (IS_ERR(us))\n\t\treturn PTR_ERR(us);',
        '\tif (IS_ERR(us))\n\t\treturn PTR_ERR(us);\n'
        '\tfor (unsigned int i = 0; i < 18; ++i)\n'
        '\t\tif (us[AM_FIRST_CHANNEL + i].id != us[AM_FIRST_CHANNEL].id + i)\n'
        '\t\t\treturn -EINVAL;\n'
        '\tio_in_it_desc.iChannelNames = us[AM_FIRST_CHANNEL].id;\n'
        '\tas_in_hdr_desc.iChannelNames = us[AM_FIRST_CHANNEL].id;\n'
        '\tusb_out_it_desc.iChannelNames = us[AM_PC_RETURN].id;\n'
        '\tas_out_hdr_desc.iChannelNames = us[AM_PC_RETURN].id;')
    # A-H are independent logical buses, not surround speaker positions.
    for descriptor in ['io_in_it_desc','as_in_hdr_desc']:
        replace(f'\t{descriptor}.bmChannelConfig = cpu_to_le32(uac2_opts->p_chmask);',
                f'\t{descriptor}.bmChannelConfig = cpu_to_le32(0);')
    replace('DECLARE_USB_FUNCTION_INIT(uac2, afunc_alloc_inst, afunc_alloc);',
            '#include "usb_names_registry.inc"')
    return source

def build(kernel,out):
    archive=kernel.parent/'linux-imb-6.12.60.tar.xz'
    assert sha(archive)==SOURCE_ARCHIVE_SHA256,'Official source archive mismatch'
    assert KERNEL_RELEASE in (kernel/'include/generated/utsrelease.h').read_text()
    assert '# CONFIG_MODVERSIONS is not set' in (kernel/'.config').read_text()
    out.mkdir(parents=True,exist_ok=True)
    function=kernel/'drivers/usb/gadget/function'
    original=function/'f_uac2.c'
    (out/'f_uac2_named.c').write_text(patched_source(original.read_text()))
    for name in ['u_uac2.h','u_audio.h','uac_common.h']:(out/name).write_bytes((function/name).read_bytes())
    (out/'usb_names_registry.inc').write_bytes((ROOT/'src/usb_names_registry.inc').read_bytes())
    (out/'Makefile').write_text('obj-m += am_usb_names.o\nam_usb_names-y := f_uac2_named.o\n')
    subprocess.run(['make','-C',str(kernel),'ARCH=arm64','CROSS_COMPILE=aarch64-linux-gnu-',f'M={out}','KBUILD_MODPOST_WARN=1','modules'],check=True)
    module=out/'am_usb_names.ko'
    info=subprocess.check_output(['aarch64-linux-gnu-readelf','-p','.modinfo',str(module)],text=True)
    assert f'vermagic={KERNEL_RELEASE} SMP preempt mod_unload aarch64' in info
    # The GPL archive omits Module.symvers. Packaging additionally requires
    # this exact binary to pass load/bind/unload on the original firmware
    # kernel in QEMU; compilation alone does not verify runtime exports.
    undefined=subprocess.check_output(['aarch64-linux-gnu-nm','-u',str(module)],text=True)
    symbols=[l.split()[-1] for l in undefined.splitlines()]
    report=dict(compiled=True,hardware_tested=False,kernel_release=KERNEL_RELEASE,
        archive_sha256=SOURCE_ARCHIVE_SHA256,original_uac2_sha256=sha(original),module_sha256=sha(module),kernel_config_sha256=sha(kernel/'.config'),
        channels=CHANNELS,undefined_symbols=symbols,
        sources_sha256={n:sha(ROOT/n) for n in ['build_usb_names.py','src/usb_names_registry.inc']})
    (out/'compile-verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(module,sha(module))

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--kernel',type=Path,default=Path('/home/user/am-mod-kernel-sources/kernel-source'))
    ap.add_argument('--out',type=Path,default=ROOT/'build/usb-names')
    args=ap.parse_args();build(args.kernel.resolve(),args.out.resolve())
