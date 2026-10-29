"""Package only generic display sources; never include configuration or map data."""
from pathlib import Path
from hashlib import sha256
from zipfile import ZipFile, ZIP_DEFLATED
import subprocess, sys
root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'tests/browser_layout_test.py')],check=True)
assets=[
 'panorama/layout/custom_game/rtv_hud/vote.xml',
 'panorama/styles/custom_game/rtv_hud/vote.css',
 'panorama/layout/custom_game/rtv_hud/preview.xml',
 'panorama/layout/custom_game/rtv_hud/browser_scroll.xml',
 'panorama/styles/custom_game/rtv_hud/browser_scroll.css',
 'Build-Windows.cmd','Build-Windows.ps1','Build-Live.cmd',
 'Build-LocalPreview.cmd','WINDOWS-START-HERE.md','LIVE.md','LOCAL-PREVIEW.md',
]
files={name:root/'workshop'/name for name in assets}
files.update({name:root/name for name in ['LICENSE','NOTICE.md','README.md']})
payload={}
for name,path in files.items():
 data=path.read_bytes()
 if path.suffix in ('.cmd','.ps1'):
  data.decode('ascii')
  data=data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')
 payload[name]=data
payload['SHA256SUMS.txt']=''.join(f'{sha256(data).hexdigest()}  {name}\n' for name,data in sorted(payload.items())).encode()
archive=root/'rtv-hud-windows-kit-v15.zip'
with ZipFile(archive,'w',ZIP_DEFLATED) as z:
 for name,data in sorted(payload.items()):z.writestr(name,data)
with ZipFile(archive) as z:
 assert z.testzip() is None and set(z.namelist())==set(payload)
 for name,data in payload.items():assert z.read(name)==data
 assert not any('maplist' in n or n.endswith(('.js','.json','.cfg')) for n in z.namelist())
print('Kit:',archive)
print('SHA256:',sha256(archive.read_bytes()).hexdigest())
print('ZIP CRC, allowlist, hashes and generic row contract passed.')
