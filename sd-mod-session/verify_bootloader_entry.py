"""Execute the shipping shell fragment with isolated service/hash fixtures."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
R=Path(__file__).resolve().parent;O=R/'build/bootloader-compat';O.mkdir(exist_ok=True)
source=R/'src/bootloader_entry.inc.sh';fragment=source.read_text();assert fragment.count('am_entry_lock=/run/am-mod-entry-bootstrap')==1
checks=[]
with tempfile.TemporaryDirectory(prefix='am-entry-') as temporary:
 root=Path(temporary);(root/'sampleboot.inplace').touch()
 script=root/'entry.sh';state=root/'service-state';log=root/'calls';lock=root/'entry-lock'
 prefix=r'''
entry_line(){ echo "$*"; }
finish_entry(){ echo "FINISH $1"; }
systemctl(){
 echo "$*" >> "$CALLS"
 if [ "$1" = is-active ]; then
  case "$*" in
   *mpc-clone*) [ "$CASE" = running ]; return;;
   *) [ "$(cat "$STATE")" = active ]; return;;
  esac
 fi
 if [ "$1" = show ]; then
  case "$*" in
   *MainPID*) if [ "$CASE" = zero-pid ]; then echo 0; else echo 7; fi;;
   *DropInPaths*) [ "$CASE" != override ] || echo injected.conf;;
   *FragmentPath*) if [ "$CASE" = fragment ]; then echo /tmp/foreign.service; else echo /usr/lib/systemd/system/ac50.service; fi;;
  esac
  return 0
 fi
 if [ "$1" = start ]; then
  [ "$CASE" != start-fails ] || return 1
  [ "$CASE" = stays-inactive ] || echo active > "$STATE"
  # Firmware runs SD autorun again when clean stock starts. Exercise the
  # same entry recursively while the outer handoff owns the bootstrap lock.
  if [ "$CASE" = recursive ]; then sh "$SELF"; fi
  return 0
 fi
 return 1
}
sleep(){ :; }
stat(){
 case "$3" in
  /proc/*) if [ "$CASE" = never-exec ]; then echo 9:9; return; fi
   if [ "$CASE" = late-exec ]; then n=$(cat "$DIR/exec-count" 2>/dev/null || echo 0);n=$((n+1));echo "$n" > "$DIR/exec-count";if [ "$n" -lt 3 ];then echo 9:9;return;fi;fi;;
 esac
 echo 1:2
}
sha256sum(){
 case "$1" in
  '/usr/bin/MPC Sample') if [ "$CASE" = hash ]; then echo bad; else echo 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508  stock'; fi;;
  *) if [ "$CASE" = unit ]; then echo bad; else echo '6b2dc52826263daf19c6f1a945c94bd7ba69b15eb7843df68eebdd63ea311e5d  unit'; fi;;
 esac
}
'''
 script.write_text(prefix+fragment.replace('am_entry_lock=/run/am-mod-entry-bootstrap','am_entry_lock="$DIR/entry-lock"')+'\necho CONTROLLER\n')
 for case in ['normal','already-active','hash','unit','override','fragment','start-fails','stays-inactive','running','recursive','busy','missing-marker','late-exec','never-exec','zero-pid']:
  state.write_text('active' if case=='already-active' else 'inactive');log.write_text('')
  if case=='busy':lock.mkdir()
  if case=='missing-marker':(root/'sampleboot.inplace').unlink()
  env=dict(os.environ,DIR=str(root),CASE=case,STATE=str(state),CALLS=str(log),SELF=str(script))
  result=subprocess.run(['sh',str(script)],env=env,capture_output=True,text=True,timeout=10)
  (O/('entry-'+case+'.log')).write_text(result.stdout+result.stderr)
  calls=log.read_text().splitlines();starts=[l for l in calls if l=='start ac50.service']
  if case in ['normal','recursive','late-exec']:assert result.returncode==0 and len(starts)==1 and result.stdout.count('CONTROLLER')==1,(case,result.stdout,calls)
  elif case=='already-active':assert result.returncode==0 and not starts and 'CONTROLLER' in result.stdout
  elif case in ['running','busy']:assert result.returncode==0 and not starts and 'CONTROLLER' not in result.stdout
  else:assert result.returncode!=0 and 'CONTROLLER' not in result.stdout
  if case in ['hash','unit','override','fragment','missing-marker']:assert not starts
  if case=='busy':lock.rmdir()
  else:assert not lock.exists(),case
  if case=='missing-marker':(root/'sampleboot.inplace').touch()
  checks.append(case)
report=dict(passed=True,checks=checks,hardware_tested=False,vm_tested=False,sources_sha256={p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in ['src/bootloader_entry.inc.sh','verify_bootloader_entry.py']})
(O/'entry-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS bootloader entry',', '.join(checks),flush=True)
