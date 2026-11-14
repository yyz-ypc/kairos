"""Stable/beta/snapshot version-generation regression; temporary headers only."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'win32/src/version.h').read_text(encoding='utf-8-sig');cases=[]
def run(text,expected):
 with tempfile.TemporaryDirectory(prefix='kairos-version-') as d:
  f=Path(d)/'version.h';f.write_text(text,encoding='utf8');p=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/'tools/check-version.ps1'),'-Header',str(f),'-SkipDocs'],capture_output=True);assert (p.returncode==0)==expected,p.stdout+p.stderr
run(source,True)
plain=re.search(r'^#define APP_VERSION_PLAIN \"([^\"]+)\"',source,re.M)[1]
beta=source.replace('#define APP_VERSION_BUILD 0','#define APP_VERSION_BUILD 2').replace('#define APP_VERSION_SUFFIX ""','#define APP_VERSION_SUFFIX "-beta"').replace(plain+'.0',plain+'.2').replace('"V'+plain+'"','"V'+plain+'-beta.2"')
for name in ['APP_VERSION_SHORT','APP_VERSION_SHORT_W']:
 beta=re.sub(r'(?m)^(#define '+name+r' L?")'+re.escape(plain)+r'"',r'\g<1>'+plain+'-beta.2"',beta)
run(beta,True);run(beta.replace('#define APP_VERSION_BUILD 2','#define APP_VERSION_BUILD 0'),False);run(source.replace('#define APP_SNAPSHOT 0','#define APP_SNAPSHOT 1'),False)
print('PASS: formal header, beta semver, beta counter rejection and formal/snapshot mismatch')
