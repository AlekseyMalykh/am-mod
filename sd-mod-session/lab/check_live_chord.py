"""Native lab-only live recording probe; never opens a physical SD device.

Run in the lab's WSL environment after selecting a loaded Poly/Tune sound.
Record the fixture's before/after native XPJ rather than trusting LED output.
"""
import gzip
import json
from pathlib import Path
import sys
import time
import urllib.request

sys.path.insert(0, '/opt/mpc-sample-lab')
import lab

URL = 'http://127.0.0.1:8765'
OUT = Path(__file__).resolve().parent.parent / 'work/lab-tests'

def request(route, payload=None):
    data = None if payload is None else json.dumps(payload).encode()
    return urllib.request.urlopen(urllib.request.Request(URL+route, data=data,
        headers={'Content-Type': 'application/json'}), timeout=15).read()

def control(name, action='press', value=None):
    p = dict(name=name, action=action)
    if value is not None:
        p['value'] = value
    request('/control', p)
    time.sleep(.3)

def tap(name):
    control(name)
    control(name, 'release')

def project():
    for _ in range(20):
        body = lab.guest('cat /run/mpc-clone-trial-v1/data-image/TempProjects/Temp-Project.xpj')
        try:
            text = gzip.decompress(body).decode()
            return json.loads(text[text.index('{'):])
        except (EOFError, gzip.BadGzipFile, ValueError):
            time.sleep(.3)
    raise RuntimeError('No complete native project snapshot')

def events(data):
    # Native cereal maps are not sorted: a full project can start with Seq120.
    # Inspect the actual selected sequence, not the first serialized entry.
    data=data['data']
    sequence=next((s['value'] for s in data['sequences'] if s['key']==data['currentSequence']),None)
    if sequence is None:
        return [] # An untouched empty sequence is omitted by native cereal.
    return [event for group in sequence['trackClipMaps'] for item in group
            for event in item['value']['eventList']['events']]

if __name__ == '__main__':
    before = project()
    tap('STOP')
    if json.loads(request('/state'))['leds'].get('80') == 3:
        tap('SEQ RECORD')
    control('SEQ RECORD')
    tap('PLAY')
    control('SEQ RECORD', 'release')
    for _ in range(20):
        leds = json.loads(request('/state'))['leds']
        if leds.get('80') == 3 and leds.get('82') == 3:
            break
        time.sleep(.1)
    else:
        raise RuntimeError('Native Seq Record + Play did not arm')
    time.sleep(10) # Native count-in on the slow software ARM CPU.
    try:
        request('/send', {'hex': '99 52 65 99 2c 4a'})
        time.sleep(.5)
        request('/send', {'hex': '89 52 00 89 2c 00'})
        time.sleep(1)
    finally:
        tap('STOP')
    time.sleep(6)
    after = project()
    stamp = time.strftime('%Y%m%d-%H%M%S')
    for name, data in [('before', before), ('after', after)]:
        (OUT / f'chord-{stamp}-{name}.json').write_text(json.dumps(data, indent=2))
    added = [e for e in events(after) if e not in events(before)]
    report = {'leds': leds, 'before_events': len(events(before)),
        'after_events': len(events(after)), 'added': added,
        'firmware': '1.3.0.12', 'physical_device_tested': False}
    (OUT / f'chord-{stamp}-report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
