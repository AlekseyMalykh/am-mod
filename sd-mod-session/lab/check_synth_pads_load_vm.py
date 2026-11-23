"""Verify native Clear -> Load of the XPJ produced by the surface-pad test.

After recording, use native New Project, capture `pads-cleared`, then use
native Load Project to reopen an unmodified copy of `pads-recorded.xpj`.
This checker only reads the app's resulting native autosave.
"""
from pathlib import Path
import hashlib,json
import check_pad_midi_engine_vm as engine
O=Path(__file__).resolve().parent.parent/'build/pad-midi-pads';engine.O=O
assert not engine.events(json.loads((O/'pads-cleared.json').read_text()))
recorded=engine.events(json.loads((O/'pads-recorded.json').read_text()))
assert len(recorded)==2
loaded=engine.events(engine.snapshot('pads-reloaded'))
assert loaded==recorded,(recorded,loaded)
proof=json.loads((O/'pads-verification.json').read_text())
assert proof['passed']
actual=engine.lab.guest('sha256sum /run/mpc-clone-trial-v1/phone-input.so',timeout=20).split()[0].decode()
assert actual==proof['library_sha256']
proof.update(native_xpj_load=True,cleared_before_load=True,native_load_notes_equal=True)
for n in ['pads-cleared.xpj','pads-reloaded.xpj']:
    proof['sha256'][n]=hashlib.sha256((O/n).read_bytes()).hexdigest()
(O/'pads-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
print('PASS surface notes: native Save -> Clear -> Load -> identical events',flush=True)
