"""Test native and firmware-compatible ARM64 proxies against fake disk services."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import time

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

ROOT = Path(__file__).parent
NAME = 'io.github.inmusicdev.edisksd'

if len(sys.argv) > 1 and sys.argv[1] == 'serve':
    DBusGMainLoop(set_as_default=True)
    bus = dbus.bus.BusConnection(sys.argv[2])
    name = dbus.service.BusName(NAME, bus=bus)

    class FakeDisk(dbus.service.Object):
        @dbus.service.method('org.freedesktop.DBus.ObjectManager', in_signature='', out_signature='a{oa{sa{sv}}}')
        def GetManagedObjects(self):
            return {'/fake': {NAME + '.Block': {'Label': dbus.String('READ_OK')}}}

        @dbus.service.method(NAME + '.Filesystem', in_signature='a{sv}', out_signature='s')
        def Mount(self, options):
            Path(sys.argv[3]).write_text('UNEXPECTED_MOUNT_CALL')
            return '/fake-only'

        @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ssv', out_signature='')
        def Set(self, interface, key, value):
            Path(sys.argv[3]).write_text('UNEXPECTED_SET_CALL')

    obj = FakeDisk(bus, '/')
    print('READY', flush=True)
    GLib.MainLoop().run()
    raise SystemExit

results = []
with tempfile.TemporaryDirectory(prefix='mpc-dbus-policy-', dir='/run') as folder:
    folder = Path(folder)
    daemon = subprocess.Popen(['dbus-daemon', '--session', '--nofork', '--print-address=1', '--nopidfile', '--nosyslog'], stdout=subprocess.PIPE, text=True)
    address = daemon.stdout.readline().strip()
    server = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), 'serve', address, str(folder/'audit')], stdout=subprocess.PIPE, text=True)
    try:
        assert server.stdout.readline().strip() == 'READY'
        for arch in ('amd64', 'arm64'):
            binary = ROOT / 'vendor/xdg-dbus-proxy' / arch / 'xdg-dbus-proxy'
            args = [str(binary)]
            if arch == 'arm64':
                args = ['qemu-aarch64', '-L', '/home/user/mpc-rootfs-user-20260808'] + args
            socket = folder / ('proxy-' + arch)
            policy = (ROOT/'proxy-policy.args').read_text().splitlines()
            with (folder/(arch+'.log')).open('w') as log:
                proxy = subprocess.Popen(args+[address,str(socket)]+policy, stdout=log, stderr=log)
                try:
                    until = time.monotonic()+8
                    while not socket.exists() and time.monotonic()<until:
                        if proxy.poll() is not None:
                            raise RuntimeError((folder/(arch+'.log')).read_text())
                        time.sleep(.05)
                    assert socket.exists(), 'No proxy socket'
                    client = dbus.bus.BusConnection('unix:path='+str(socket))
                    remote = client.get_object(NAME, '/', introspect=False)
                    objects = remote.GetManagedObjects(dbus_interface='org.freedesktop.DBus.ObjectManager', timeout=5)
                    assert objects['/fake'][NAME+'.Block']['Label'] == 'READ_OK'
                    denied = []
                    for method, interface, parameters in [
                        ('Mount', NAME+'.Filesystem', (dbus.Dictionary({},signature='sv'),)),
                        ('Set', 'org.freedesktop.DBus.Properties', (NAME+'.Block','Label',dbus.String('CHANGED',variant_level=1)))]:
                        try:
                            remote.get_dbus_method(method,interface)(*parameters, timeout=5)
                        except dbus.DBusException as error:
                            assert error.get_dbus_name() in ('org.freedesktop.DBus.Error.AccessDenied','org.freedesktop.DBus.Error.ServiceUnknown')
                            denied.append(method)
                        else:
                            raise AssertionError('Mutating call allowed: '+method)
                    assert not (folder/'audit').exists(), 'Mutating call reached fake service'
                    client.close()
                    result=dict(arch=arch,read='PASS',denied=denied,mutating_calls_reached_service=0)
                    results.append(result)
                    print(json.dumps(result),flush=True)
                finally:
                    proxy.terminate()
                    proxy.wait(timeout=5)
    finally:
        server.terminate()
        server.wait(timeout=5)
        daemon.terminate()
        daemon.wait(timeout=5)
(ROOT/'dbus-policy-results.json').write_text(json.dumps(results,indent=2)+'\n')
