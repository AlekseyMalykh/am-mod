"""Read-only pinned ELF writer inventory. This does NOT certify writer coverage."""
from pathlib import Path
import argparse,hashlib,json,struct
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve();b=stock.read_bytes()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(b).hexdigest()==expected
phoff=struct.unpack_from('<Q',b,32)[0]
segments=[struct.unpack_from('<IIQQQQQQ',b,phoff+i*56) for i in range(struct.unpack_from('<H',b,56)[0])]
code=next(s for s in segments if s[0]==1 and s[1]&1)
def file_offset(va):return next(s[2]+va-s[3] for s in segments if s[0]==1 and s[3]<=va<s[3]+s[5])
def word(va):return struct.unpack_from('<I',b,file_offset(va))[0]
def ptr(va):return struct.unpack_from('<Q',b,file_offset(va))[0]
targets={0x29c9490:'Program assignment',0x2957ae0:'Sequence Clip list getter',0x29579e0:'Sequence Clip map selector lookup',0x27ee5d0:'Event list insertion',0x29589f0:'Sequence destructor',0x29112f0:'Project destructor',0x290cee0:'Project teardown'}
read_only={0x2957ae0,0x29579e0}
calls={v:[] for v in targets.values()}
for i,(w,) in enumerate(struct.iter_unpack('<I',b[code[2]:code[2]+code[5]-code[5]%4])):
    kind=w>>26
    if kind not in (5,37):continue
    imm=w&0x3ffffff
    if imm&(1<<25):imm-=1<<26
    va=code[3]+i*4;target=va+imm*4
    if target in targets:calls[targets[target]].append(dict(site=hex(va),branch='BL' if kind==37 else 'B',target_read_only=target in read_only))
# STR Xt,[Xn,#0xbb8] in the native Track code range. Register-base identity
# and other encodings must be audited separately; these are candidates only.
stores=[va for va in range(0x29c0000,0x29ca000,4) if word(va)&0xffc00000==0xf9000000 and (word(va)>>10)&0xfff==0xbb8//8]
assert stores==[0x29c7b38,0x29c9440,0x29c9518,0x29c9884,0x29c98e0,0x29c9de0]
assert word(0x29c9490)==0xd503233f and word(0x29c9518)==0xf905df53
assert ptr(0x6ecc648)==0x29112f0 and ptr(0x6ecdff0)==0x29589f0
def hint(va):
    for at in range(va, max(code[3],va-0x4000)-1,-4):
        if word(at) in (0xd503233f,0xd503245f):return hex(at)
    return None
probe_path=R/'build/pattern-writer/verification.json';probe=None
if probe_path.exists():
    candidate=json.loads(probe_path.read_text())
    if candidate.get('passed') and candidate.get('stock_sha256')==expected and all(hashlib.sha256((R/f).read_bytes()).hexdigest()==h for f,h in candidate['sources_sha256'].items()):
        probe=dict(proof_sha256=hashlib.sha256(probe_path.read_bytes()).hexdigest(),checks=candidate['checks'],program_entry='0x29c9490',program_disappearance=candidate.get('actual_program_disappearance',[]),program_replacement=candidate.get('actual_program_replacement'),replacement_scope=candidate.get('replacement_scope'),replacement_subtype_executed=candidate.get('replacement_subtype_executed',False),clip_argument='map selector, not create',observer_services='fixtures',thread_affinity_certified=False)
executed={0x29c7b38:('0x29c7a80','Track+0x910'),0x29c9440:('0x29c93a0','Track'),0x29c9518:('0x29c9490','Track')}
event_path=R/'build/pattern-event-writer/verification.json';event_probe=None
if event_path.exists():
    candidate=json.loads(event_path.read_text())
    if candidate.get('passed') and candidate.get('stock_sha256')==expected and all(hashlib.sha256((R/f).read_bytes()).hexdigest()==h for f,h in candidate['sources_sha256'].items()):
        event_probe=dict(proof_sha256=hashlib.sha256(event_path.read_bytes()).hexdigest(),checks=candidate['checks'],entries=candidate['actual_event_entries'],event_variant=candidate['event_variant'],bulk_scope=candidate['bulk_scope'],notification_services='fixtures',thread_affinity_certified=False,hook_ready=False)
if probe and probe.get('program_replacement')=='0x29c9670':
    receiver='Track; base and subtype branches' if probe.get('replacement_subtype_executed') else 'Track; base Program path'
    executed.update({0x29c9884:('0x29c9670',receiver),0x29c98e0:('0x29c9670',receiver)})
report=dict(stock_sha256=expected,analyzer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),coverage_complete=False,production_read_admission=False,
    native_contract_probe=probe,native_event_contract_probe=event_probe,read_only_targets=[hex(x) for x in sorted(read_only)],mutator_direct_branches=sum(len(calls[name]) for target,name in targets.items() if target not in read_only),
    program_field_store_candidates=[dict(site=hex(va),instruction=hex(word(va)),entry_hint=hint(va),hint_certified=False,executed_entry=executed[va][0] if probe and va in executed else None,receiver=executed[va][1] if probe and va in executed else None,hook_ready=False) for va in stores],
    direct_branch_inventory=calls,
    remaining=['classify constructors/destructors and replacement paths','indirect/virtual callbacks and adjusted-base stores','event erase/clear/bulk changes and history','thread affinity and lock order','actual async completion and source rebind'],
    note='Read-only inventory; no hooks installed, no application build, direct branches are not complete call coverage')
O=R/'build/pattern-writers';O.mkdir(exist_ok=True);(O/'inventory.json').write_text(json.dumps(report,indent=2)+'\n')
print('Pinned writer inventory:',len(stores),'Program-field store candidates;',sum(map(len,calls.values())),'direct branches; coverage_complete=false',flush=True)
