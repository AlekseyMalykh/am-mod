"""Build, package and publish Am MOD r27q on GitHub (run in WSL from sd-mod-session).

  python3 publish_r27q.py --dry-run   # build and prepare releases/r27q, change nothing online
  python3 publish_r27q.py             # also push main + tag r27q, create the release,
                                      # update the repository About and older release notes

Uses `gh`/`git` from WSL, or the Windows `gh.exe`/`git.exe` when WSL has no login."""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys,datetime
R=Path(__file__).resolve().parent;ROOT=R.parent;REPO='AlekseyMalykh/am-mod';TAG='r27q'
TITLE='Am MOD r27q — Degrees, search and window polish'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
ap=argparse.ArgumentParser();ap.add_argument('--dry-run',action='store_true');ap.add_argument('--prerelease',action='store_true');ap.add_argument('--skip-build',action='store_true')
args=ap.parse_args()
def run(cmd,**k):print('+',' '.join(map(str,cmd)),flush=True);return subprocess.run(list(map(str,cmd)),check=True,**k)
def out(cmd):return subprocess.run(list(map(str,cmd)),check=True,capture_output=True,text=True).stdout.strip()
def tool(name):
    for c in (name,name+'.exe'):
        if shutil.which(c):
            if c==name and name=='gh' and subprocess.run([c,'auth','status'],capture_output=True).returncode:continue
            return c
    sys.exit(f'{name} not found or not logged in (gh auth login)')
def win(p,exe):return out(['wslpath','-w',p]) if exe.endswith('.exe') else str(p)

# 1. Clean tree, fast-forward from origin/main.
run(['git','fetch','origin'],cwd=ROOT)
status=out(['git','-C',ROOT,'status','--porcelain','--untracked-files=no'])
if status:sys.exit('Commit or stash changes first:\n'+status)
head=out(['git','-C',ROOT,'rev-parse','HEAD'])
if subprocess.run(['git','-C',str(ROOT),'merge-base','--is-ancestor','origin/main','HEAD']).returncode:sys.exit('HEAD does not contain origin/main; rebase first')
# 2. Build and package from this exact commit.
if not args.skip_build:
    for s in ['build_chords_candidate.py','verify_early_drm.py','package_r27q.py']:run(['python3',s],cwd=R)
z=R/'build/r27q/am-mod-r27q.zip'
# 3. Release folder.
D=ROOT/'releases'/TAG;D.mkdir(parents=True,exist_ok=True)
installer=ROOT/'releases/r27f/MPC-Sample-Mod-Installer.exe'
assert sha(installer)=='e6383828180278481904180afbc263c6c812df8c26f51fd05f17bdb3f46b8b90','unexpected installer 1.0.3'
shutil.copy2(z,D/z.name);shutil.copy2(installer,D/installer.name)
app=json.loads((R/'build/chords/compile-verification.json').read_text());drm=json.loads((R/'build/early-drm/verification.json').read_text())
ver=dict(revision=TAG,source_commit=head,package='am-mod-r27q.zip',package_sha256=sha(z),base='am-mod-r27p.zip',
         base_sha256='7effb3e544892bfbf703a98fae7b5fa33ae8b912318af6e455fd82fb89c667cb',
         checks=dict(full_preload_constructor=app['full_preload_constructor'],library_sha256=app['library_sha256'],drm_splash=drm['passed'],splash_sha256=drm['splash_sha256'],
                     host_tests=['check_music_lights.c','check_branding.c','check_phone_delete.c','check_phone_cursor.c']),
         hardware_tested='earlier r27q builds by the author; final build pending',built_at=datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'))
(D/'release-verification-r27q.json').write_text(json.dumps(ver,indent=2)+'\n')
assets=[D/'am-mod-r27q.zip',D/'MPC-Sample-Mod-Installer.exe',D/'release-verification-r27q.json']
(D/'SHA256SUMS.txt').write_text(''.join(f'{sha(a)}  {a.name}\n' for a in assets));assets.append(D/'SHA256SUMS.txt')
en=(ROOT/'docs/RELEASE_NOTES-r27q.md').read_text().split('\n',1)[1].strip();ru=(ROOT/'docs/RELEASE_NOTES-r27q.ru.md').read_text().split('\n',1)[1].strip()
fix=lambda t:t.replace('](KNOWN_ISSUES',f'](https://github.com/{REPO}/blob/{TAG}/docs/KNOWN_ISSUES').replace('](RELEASE_NOTES',f'](https://github.com/{REPO}/blob/{TAG}/docs/RELEASE_NOTES')
body=f"{fix(en)}\n\nSHA-256 `am-mod-r27q.zip`: `{sha(z)}`\n\n---\n\n## По-русски\n\n{fix(ru)}\n"
(D/'release-body.md').write_text(body)
print('Prepared',D,'package',sha(z))
if args.dry_run:print('Dry run: nothing pushed or published.');sys.exit(0)
# 4. Push main and the tag.
git='git'
if subprocess.run(['git','-C',str(ROOT),'push','origin','HEAD:main']).returncode:
    git='git.exe';run([git,'-C',win(ROOT,git),'push','origin','HEAD:main'])
if not subprocess.run(['git','-C',str(ROOT),'rev-parse','-q','--verify','refs/tags/'+TAG],capture_output=True).returncode==0:
    who=[out(['git','-C',ROOT,'log','-1','--format=%an']),out(['git','-C',ROOT,'log','-1','--format=%ae'])]
    run(['git','-C',ROOT,'-c','user.name='+who[0],'-c','user.email='+who[1],'tag','-a',TAG,'-m',TITLE,head])
run([git,'-C',win(ROOT,git),'push','origin',TAG])
# 5. Release.
gh=tool('gh')
flags=['--prerelease'] if args.prerelease else ['--latest']
run([gh,'release','create',TAG,*[win(a,gh) for a in assets],'--repo',REPO,'--title',TITLE,'--notes-file',win(D/'release-body.md',gh),'--verify-tag',*flags])
# 6. Repository About.
run([gh,'repo','edit',REPO,'--description','Unofficial SD-card mod for the Akai MPC Sample: step sequencer, chords, scale layouts, search, pad EQ, resampling, rear recording. Nothing is flashed.',
     '--homepage',f'https://github.com/{REPO}/releases/latest',*sum([['--add-topic',t] for t in ['akai','mpc','mpc-sample','sampler','firmware-mod','music-production','sd-card']],[])])
# 7. Older releases: one uniform note at the top, Am MOD titles, tag links.
notes={'r24x':'> **Outdated.** Use the [latest release](https://github.com/AlekseyMalykh/am-mod/releases/latest).',
       'r25x':'> **Outdated.** Use the [latest release](https://github.com/AlekseyMalykh/am-mod/releases/latest).',
       'r26a':'> **Outdated.** FAT32 is supported experimentally since r26e. Use the [latest release](https://github.com/AlekseyMalykh/am-mod/releases/latest).',
       'r26d':'> **Outdated.** FAT32 is supported experimentally since r26e. Use the [latest release](https://github.com/AlekseyMalykh/am-mod/releases/latest).',
       'r26s':'> **Older stable release.** The [latest release](https://github.com/AlekseyMalykh/am-mod/releases/latest) is r27q.',
       'r27f':'> **Superseded by [r27q](https://github.com/AlekseyMalykh/am-mod/releases/tag/r27q).**',
       'r27n':'> **Superseded by [r27q](https://github.com/AlekseyMalykh/am-mod/releases/tag/r27q).**',
       'r27p':'> **Previous release.** The [latest release](https://github.com/AlekseyMalykh/am-mod/releases/latest) is r27q.'}
for tag,note in notes.items():
    info=json.loads(out([gh,'release','view',tag,'--repo',REPO,'--json','body,name']))
    text=info['body'] or ''
    if not text.startswith(note):text=note+'\n\n'+text
    if tag=='r26s':text=text.replace(f'/{REPO}/blob/main/',f'/{REPO}/blob/r26s/')
    title=info['name'].replace('AM Mod','Am MOD')
    f=D/f'old-{tag}.md';f.write_text(text)
    run([gh,'release','edit',tag,'--repo',REPO,'--title',title,'--notes-file',win(f,gh)])
(D/'publication-receipt.json').write_text(json.dumps(dict(published=True,tag=TAG,commit=head,release_url=f'https://github.com/{REPO}/releases/tag/{TAG}',package_sha256=sha(z)),indent=2)+'\n')
print('Published',f'https://github.com/{REPO}/releases/tag/{TAG}')
