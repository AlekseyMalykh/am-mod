"""Read MPC descriptors through Windows USB hub GET_DESCRIPTOR (no writes).

Uses the Microsoft USBView IOCTL interface. No driver installation, USB
configuration changes, stream start, or REAPER configuration access.
"""
import ctypes as C
from ctypes import wintypes as W
import json, struct, uuid
from pathlib import Path

class GUID(C.Structure):
    _fields_=[('data',C.c_ubyte*16)]
class INTERFACE(C.Structure):
    _fields_=[('size',W.DWORD),('guid',GUID),('flags',W.DWORD),('reserved',C.c_size_t)]
setup=C.WinDLL('setupapi',use_last_error=True)
kernel=C.WinDLL('kernel32',use_last_error=True)
setup.SetupDiGetClassDevsW.argtypes=[C.POINTER(GUID),W.LPCWSTR,W.HWND,W.DWORD]
setup.SetupDiGetClassDevsW.restype=W.HANDLE
setup.SetupDiEnumDeviceInterfaces.argtypes=[W.HANDLE,C.c_void_p,C.POINTER(GUID),W.DWORD,C.POINTER(INTERFACE)]
setup.SetupDiGetDeviceInterfaceDetailW.argtypes=[W.HANDLE,C.POINTER(INTERFACE),C.c_void_p,W.DWORD,C.POINTER(W.DWORD),C.c_void_p]
setup.SetupDiDestroyDeviceInfoList.argtypes=[W.HANDLE]
kernel.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,C.c_void_p,W.DWORD,W.DWORD,W.HANDLE]
kernel.CreateFileW.restype=W.HANDLE
kernel.DeviceIoControl.argtypes=[W.HANDLE,W.DWORD,C.c_void_p,W.DWORD,C.c_void_p,W.DWORD,C.POINTER(W.DWORD),C.c_void_p]
kernel.CloseHandle.argtypes=[W.HANDLE]
INVALID=C.c_void_p(-1).value

def hub_paths():
    guid=GUID((C.c_ubyte*16).from_buffer_copy(uuid.UUID('f18a0e88-c30c-11d0-8815-00a0c906bed8').bytes_le))
    devices=setup.SetupDiGetClassDevsW(C.byref(guid),None,None,0x12)
    if devices==INVALID:raise C.WinError(C.get_last_error())
    try:
        i=0
        while True:
            interface=INTERFACE();interface.size=C.sizeof(interface)
            if not setup.SetupDiEnumDeviceInterfaces(devices,None,C.byref(guid),i,C.byref(interface)):break
            needed=W.DWORD()
            setup.SetupDiGetDeviceInterfaceDetailW(devices,C.byref(interface),None,0,C.byref(needed),None)
            buffer=C.create_string_buffer(needed.value);struct.pack_into('<I',buffer,0,8 if C.sizeof(C.c_void_p)==8 else 6)
            if not setup.SetupDiGetDeviceInterfaceDetailW(devices,C.byref(interface),buffer,len(buffer),C.byref(needed),None):raise C.WinError(C.get_last_error())
            yield C.wstring_at(C.addressof(buffer)+4)
            i+=1
    finally:setup.SetupDiDestroyDeviceInfoList(devices)

def ioctl(handle,code,data,size):
    buffer=C.create_string_buffer(size);C.memmove(buffer,data,len(data));returned=W.DWORD()
    if not kernel.DeviceIoControl(handle,code,buffer,len(data),buffer,size,C.byref(returned),None):raise C.WinError(C.get_last_error())
    return buffer.raw[:returned.value]

def descriptor(handle,port,kind,index=0,lang=0):
    request=struct.pack('<IBBHHH',port,0x80,6,(kind<<8)|index,lang,4096)
    return ioctl(handle,0x220410,request,4108)[12:]

def inspect():
    failures=[]
    for path in hub_paths():
        handle=kernel.CreateFileW(path,0,3,None,3,0,None)
        if handle==INVALID:
            failures.append({'hub_open_error':C.get_last_error()});continue
        try:
            node=ioctl(handle,0x220408,bytes(4),1024)
            for port in range(1,node[6]+1):
                try:connection=ioctl(handle,0x220448,struct.pack('<I',port),4096)
                except OSError:continue
                if len(connection)<22 or struct.unpack_from('<HH',connection,12)!=(0x09e8,0x205c):continue
                config=descriptor(handle,port,2);total=struct.unpack_from('<H',config,2)[0];config=config[:total]
                strings={}
                for index in range(1,40):
                    try:
                        b=descriptor(handle,port,3,index,0x409)
                        if len(b)>=2 and b[1]==3:strings[index]=b[2:b[0]].decode('utf-16-le')
                    except OSError:pass
                audio=[];interface=None;pos=0
                while pos+2<=len(config):
                    size,kind=config[pos:pos+2]
                    if size<2 or pos+size>len(config):raise ValueError('Invalid descriptor length')
                    d=config[pos:pos+size]
                    if kind==4 and size>=9:interface={'number':d[2],'alt':d[3],'class':d[5],'subclass':d[6]}
                    if kind==0x24 and interface and interface['class']==1:
                        if interface['subclass']==2 and size>=16 and d[2]==1:
                            audio.append({**interface,'type':'stream','channels':d[10],'channel_mask':struct.unpack_from('<I',d,11)[0],'channel_names_index':d[15]})
                        elif interface['subclass']==1 and size>=17 and d[2]==2:
                            audio.append({**interface,'type':'input-terminal','terminal':d[3],'channels':d[8],'channel_mask':struct.unpack_from('<I',d,9)[0],'channel_names_index':d[13]})
                    pos+=size
                return {'found':True,'audio':audio,'strings':strings,'configuration_hex':config.hex(),'read_only':True}
        finally:kernel.CloseHandle(handle)
    return {'found':False,'errors':failures,'read_only':True}

if __name__=='__main__':
    import sys
    report=inspect()
    if len(sys.argv)>1:Path(sys.argv[1]).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='configuration_hex'},ensure_ascii=True,indent=2))
