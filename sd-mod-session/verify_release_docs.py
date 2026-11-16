"""Check current release documentation, local links and generated FX media."""
from pathlib import Path
import hashlib,json,re
from urllib.parse import unquote
R=Path(__file__).resolve().parent.parent
names=['README.md','README.ru.md','NOTICE.md','docs/README.md','docs/BUILD.md',
 'docs/USER_GUIDE.md','docs/USER_GUIDE.ru.md','docs/USB_GUIDE.md','docs/USB_GUIDE.ru.md',
 'docs/RELEASE_NOTES-r30b.md','docs/RELEASE_NOTES-r30b.ru.md','docs/FX_GALLERY.md',
 'docs/FX_KNOB_LAYOUT.ru.md','docs/AMP_VOICES.ru.md']
errors=[];links=0
for name in names:
 text=(R/name).read_text(encoding='utf-8')
 assert '\ufffd' not in text,name
 targets=re.findall(r'\]\(([^)]+)\)',text)+re.findall(r'<img\b[^>]*\bsrc="([^"]+)"',text)
 for target in targets:
  if re.match(r'\w+://|#|mailto:',target):continue
  path=unquote(target.split('#',1)[0])
  if not (R/name).parent.joinpath(path).exists():errors.append((name,target))
  links+=1
assert not errors,errors
from PIL import Image
gifs=['tape-delay','tape','vinyl','shimmer-reverb','delay','diff-delay','spring-reverb',
 'phaser','flanger','optocat','amp-models','bass-amp']
for name in gifs:
 with Image.open(R/'docs/media/fx'/(name+'.gif')) as im:
  assert im.size==(640,480) and im.n_frames>1,(name,im.size,im.n_frames)
  im.seek(im.n_frames-1);im.load()
O=R/'sd-mod-session/build/release-docs';O.mkdir(parents=True,exist_ok=True)
(O/'verification.json').write_text(json.dumps(dict(passed=True,links_checked=links,
 animated_gifs=len(gifs),sources_sha256={name:hashlib.sha256((R/name).read_bytes()).hexdigest()
 for name in names}),indent=2)+'\n')
print(f'PASS current EN/RU documentation: {links} local links, UTF-8, {len(gifs)} animated 640x480 GIFs')
