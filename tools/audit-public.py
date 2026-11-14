"""Fail closed before a source push. Report locations, never matched secret values."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--tracked',action='store_true');p.add_argument('--private-reference',action='append',type=Path,default=[]);a=p.parse_args();errors=[];files=0;total=0
private_ids=set();private_phrases=set()
for source in a.private_reference:
 text=source.read_text(encoding='utf-8-sig');private_ids.update(re.findall(r'(?<![0-9])[1-9][0-9]{6,11}(?![0-9])',text));private_phrases.update(line.strip()[:96] for line in text.splitlines() if len(line.strip())>=60)
patterns=[r'gh[pousr]_[A-Za-z0-9]{30,}',r'github_pat_[A-Za-z0-9_]{40,}',r'sk-[A-Za-z0-9]{24,}',r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----',r'SESSDATA=[A-Za-z0-9%]{24,}']
candidates=[a.root/x for x in subprocess.check_output(['git','-C',str(a.root),'ls-files','-z']).decode('utf8').split(chr(0)) if x] if a.tracked else a.root.rglob('*')
for f in candidates:
 if not f.is_file() or '.git' in f.relative_to(a.root).parts:continue
 s=f.relative_to(a.root).as_posix();files+=1;size=f.stat().st_size;total+=size
 if size>=50*1024*1024:errors.append([s,'oversized file'])
 if s.startswith('sidecar/') or f.suffix.lower() in ['.exe','.dll','.whl','.zip','.wasm','.jsc']:errors.append([s,'runtime binary/source in git'])
 if any(x in f.relative_to(a.root).parts for x in ['private','secrets','bots','node_modules']) or f.name in ['settings.json','answers.json','credentials.json','.env']:errors.append([s,'local/private data'])
 if s.endswith(('.png','.jpg','.webp','.ico')):continue
 try:text=f.read_text(encoding='utf-8-sig')
 except UnicodeDecodeError:errors.append([s,'unexpected binary']);continue
 if any(value in text for value in private_ids) or any(value in text for value in private_phrases):errors.append([s,'private reference overlap'])
 for pat in patterns:
  if re.search(pat,text):errors.append([s,'credential pattern'])
 if s=='.gitattributes' and 'filter=lfs' in text:errors.append([s,'LFS enabled'])
if a.tracked and files==0:errors.append(['git-index','no files to audit'])
result=dict(passed=not errors,files=files,bytes=total,errors=errors)
print(json.dumps(result,ensure_ascii=False,indent=2));raise SystemExit(0 if not errors else 1)
