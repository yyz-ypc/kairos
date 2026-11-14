"""Generate SHA256SUMS and strict channel manifests from finished local assets."""
import argparse,hashlib,json,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--dir',type=Path,default=Path('dist'));p.add_argument('--version',default='V1.0.0');p.add_argument('--channel',choices=['stable','beta'],default='stable');p.add_argument('--min-version',default='1.0.0');p.add_argument('--previous',type=Path);a=p.parse_args()
if not re.fullmatch(r'V\d+\.\d+\.\d+(-beta\.\d+)?',a.version) or (a.channel=='stable' and '-beta.' in a.version):raise SystemExit('Invalid channel/version')
assets=[]
for f in sorted(a.dir.iterdir()):
 if f.suffix not in ['.exe','.zip']:continue
 sha=hashlib.file_digest(f.open('rb'),'sha256').hexdigest();assets.append(dict(name=f.name,url=f'https://github.com/YYZ-YPC/kairos/releases/download/{a.version}/{f.name}',sha256=sha,size=f.stat().st_size))
setup='Kairos-'+a.version[1:]+'-setup.exe'
if not any(x['name']==setup for x in assets):raise SystemExit('Matching installer missing')
m=dict(schema=1,version=a.version,channel=a.channel,min_version=a.min_version,assets=assets,release=f'https://github.com/YYZ-YPC/kairos/releases/tag/{a.version}',previous=None)
if a.previous:
 prev=json.loads(a.previous.read_text(encoding='utf-8-sig'));prev.pop('previous',None)
 if prev['channel']!=a.channel:raise SystemExit('Previous channel mismatch')
 m['previous']=prev
(a.dir/'latest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
(a.dir/'SHA256SUMS').write_text(''.join(x['sha256']+'  '+x['name']+'\n' for x in assets),encoding='ascii')
for x in assets:
 if x['name'].endswith('.exe'):(a.dir/(x['name']+'.sha256')).write_text(x['sha256']+'  '+x['name']+'\n',encoding='ascii')
print('Release manifest generated:',a.channel,a.version,len(assets),'assets')
