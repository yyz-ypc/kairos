"""Create deterministic release archives and a per-file SHA256 catalog. No network."""
import argparse,hashlib,json,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--version',default='1.0.0');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
def digest(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
groups={'hermes':['sidecar/hermes'],'rift':['sidecar/rift','sidecar/rift.config.template.json'],'node':['sidecar/node'],'keeper':['sidecar/keeper'],'webview2':['bin/WebView2Loader.dll'],'vendor':['pack/mingling-vendor']}
assets=[]
for id,roots in groups.items():
 files=[]
 for rel in roots:
  p=a.root/rel
  if p.is_dir():files.extend(p.rglob('*'))
  elif p.is_file():files.append(p)
 files=sorted({p for p in files if p.is_file() and not any(x in p.parts for x in ['__pycache__','.git','state','logs','.selftest'])})
 if not files:continue
 rows=[dict(path=p.relative_to(a.root).as_posix(),size=p.stat().st_size,sha256=digest(p)) for p in files]
 name=f'kairos-{id}-{a.version}.zip';dest=a.out/name
 with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_STORED) as z:
  for p in files:
   info=zipfile.ZipInfo(p.relative_to(a.root).as_posix(),(2026,11,14,0,0,0));info.external_attr=0o100644<<16;z.writestr(info,p.read_bytes())
 assets.append(dict(id=id,name=name,url=f'https://github.com/YYZ-YPC/kairos/releases/download/V{a.version}/{name}',sha256=digest(dest),size=dest.stat().st_size,files=rows))
(a.root/'tools').mkdir(exist_ok=True)
(a.root/'tools/bundled-assets.json').write_text(json.dumps(dict(schema=1,version='V'+a.version,assets=assets),ensure_ascii=False,indent=2),encoding='utf8')
print('Bundled archives:',len(assets),'files:',sum(len(a['files']) for a in assets))
