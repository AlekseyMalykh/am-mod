"""Execute the shipped supervisor's exact phase-poll block without sleeps/device."""
from pathlib import Path
import subprocess,tempfile
R=Path(__file__).resolve().parent
s=(R/'src/autorun.template.sh').read_text()
a=s.index('        # Only a picker pad gesture arms this recovery')
b=s.index('        n=$((n+1))',a)
block=s[a:b]
assert 'picker-trace.txt' in s and 'picker_busy" -ge 15' in block
def check(lines):
 with tempfile.TemporaryDirectory(prefix='picker-watch-') as d:
  script='set -eu\nAPP=$1\npicker_seen= picker_observed_serial= picker_busy=0\nlog(){ echo "$*"; }\nexport_report(){ echo EXPORTED; }\nwhile IFS= read -r phase; do\nprintf "%s\\n" "$phase" > "$APP/picker-phase.txt"\n'+block+'done\n'
  return subprocess.run(['sh','-c',script,'test',d],input='\n'.join(lines)+'\n',capture_output=True,text=True,check=True).stdout
assert 'TIMEOUT' not in check(['PAD_DONE 1']*100)
assert 'TIMEOUT' not in check(['PAD_BUSY 1']*14+['PAD_DONE 1']*100)
assert 'PICKER_PAD_CALLBACK_TIMEOUT' in check(['PAD_BUSY 1']*15)
assert 'PICKER_PAD_CALLBACK_TIMEOUT' in check(['PAD_SELECTED 1']*15)
assert 'PICKER_PAD_CALLBACK_TIMEOUT' in check(['AUDIO_WAIT 1']*15)
assert 'TIMEOUT' not in check(['PAD_BUSY 1']*14+['PAD_BUSY 2']*14+['PAD_DONE 2'])
out=check(['PAD_DONE 1']*10);assert out.count('EXPORTED')==1
print('PASS shipped picker phase poll: scoped15 checks; Done/normal use never times out, new gesture resets count, only changed phases export report')
