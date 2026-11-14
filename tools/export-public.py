"""Create a new reviewed source tree without private runtime/source/history. Never delete originals."""
import argparse,shutil,json,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=Path.cwd();out=a.out.resolve()
if out.exists():raise SystemExit('Output must be a new directory')
allowed={'win32','tools','installer','skill','acceptance','docs','art','plugin','pack','.github'}
rootfiles={'README.md','CHANGELOG.md','DELIVERY.md','LICENSE','.gitignore','.gitattributes','start-hermes.cmd','start-keeper.cmd','plugin-selftest.py','时序之子.bat'}
excluded=[];copied=[]
for f in root.rglob('*'):
 if not f.is_file():continue
 rel=f.relative_to(root);s=rel.as_posix()
 if rel.parts[0] not in allowed and s not in rootfiles:continue
 reason=''
 if any(x in rel.parts for x in ['build','node_modules','__pycache__','.git','.tmp','out','evidence','stage','cache','.pytest_cache','data','state','logs']):reason='generated/local data'
 if f.suffix.lower() in ['.exe','.dll','.whl','.zip','.wasm','.o','.obj','.pdb','.a','.lib','.pyc','.wav','.mp3','.flac','.ogg','.bak','.tmp','.log']:reason='binary/runtime/private voice'
 if s.startswith(('docs/heart/','art/themes/','art/candidates/','tools/python/','tools/dl/','tools/innosetup','tools/innoextract/','tools/nsis/','pack/login/','pack/mingling-vendor/','plugin/mingling_skill/','pack/mingling-pack/voice/')):reason='private or release asset'
 if 'sing_server/voice/' in s or f.name in ['build.local.bat','skill-home.txt','skill-version.json','version_auto.iss','brand_auto.iss','login_auto.iss']:reason='private/generated'
 if s.startswith('art/') and not s.startswith(('art/styles/','art/character/')) and s!='art/SOURCES.md':reason='legacy private artwork'
 if reason:excluded.append(dict(path=s,reason=reason));continue
 target=out/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target);copied.append(s)
# Generic configuration preserves stable internal paths and all feature/plugin names.
pack=out/'pack/mingling-pack/pack.json';m=json.loads(pack.read_text(encoding='utf-8-sig'))
def neutral(x):
 if isinstance(x,dict):return {k:neutral(v) for k,v in x.items()}
 if isinstance(x,list):return [neutral(v) for v in x]
 if isinstance(x,str):return x.replace('公孙冥零','旅伴').replace('冥零','旅伴')
 return x
m=neutral(m);m['name']='Kairos 通用配置包';m['persona']={'id':'Kairos-Assistant','injector':'tools/import-mingling.ps1','manifest':'plugin/mingling_skill/mingling_skill.json','file':'../../plugin/mingling_skill/persona/mingling.md'}
if isinstance(m.get('voice'),dict):
 for key in list(m['voice']):
  v=m['voice'][key]
  if isinstance(v,dict):
   for field in ['voice_id','speaker','speaker_id','reference_audio','prompt_audio','reference_text','prompt_text']: 
    if field in v:v[field]=''
   if 'persona' in v:v['persona']='自然、清晰地说话'
pack.write_text(json.dumps(m,ensure_ascii=False,indent=2),encoding='utf8')
persona=out/'plugin/mingling_skill';(persona/'persona').mkdir(parents=True,exist_ok=True)
(persona/'persona/mingling.md').write_text('# Kairos 通用助手\n\n你是用户自行配置的助手。清楚、诚实地回应；不知道时明确说明。保护用户隐私，不向其他人泄露凭据或私人信息。用户可以在此替换自己的角色设定。\n',encoding='utf8')
(persona/'mingling_skill.json').write_text(json.dumps({'id':'mingling_skill','name':'Kairos 通用角色模板','display_name':'自定义角色','version':'1.0.0','schema':1,'persona':{'id':'Kairos-Assistant','file':'persona/mingling.md','encoding':'utf-8'},'placeholders':{'persona_id':'Kairos-Assistant','bot_name':'助手','owner_qq':'','owner_name':'','user_persona':''}},ensure_ascii=False,indent=2),encoding='utf8')
# Only the current reviewed evidence is exported, never the original account screenshots/logs.
for rel in ['evidence/matrix','evidence/release','evidence/render-matrix.json']:
 src=root/rel
 if src.is_dir():shutil.copytree(src,out/rel,dirs_exist_ok=True)
 elif src.is_file():(out/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,out/rel)
(out/'evidence/release').mkdir(parents=True,exist_ok=True)
(out/'evidence/release/exclusions.json').write_text(json.dumps({'excluded_categories':['original rift/hermes source and private Git history','persona and original character artwork','QQ login packages and sessions','account/cookie/token/local settings','voice references and personal prompts','runtime binaries and wheels (Release assets only)','old account screenshots and developer logs'],'excluded_files':excluded},ensure_ascii=False,indent=2),encoding='utf8')
print('Public source export:',out,'files:',len(copied),'excluded:',len(excluded))
