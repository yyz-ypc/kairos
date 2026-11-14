"""Real PowerShell/curl integration against a local Range-capable 20 MiB source."""
import sys,copy,hashlib,http.server,json,os,pathlib,subprocess,threading,time,shutil,uuid
ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'evidence/component-supply';OUT.mkdir(exist_ok=True)
WORK=ROOT/'.tmp/component-supply'/uuid.uuid4().hex
WORK.mkdir(parents=True,exist_ok=True)
DATA=bytes(range(256))*81920 # 20 MiB
SHA=hashlib.sha256(DATA).hexdigest()
original=json.loads((ROOT/'tools/components.json').read_text(encoding='utf-8-sig'))
requests=[];mode={'bad':False,'rate':1024*1024,'huge':False}
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*a):pass
 def do_HEAD(self):self.serve(True)
 def do_GET(self):self.serve(False)
 def serve(self,head):
  if self.path=='/components.json':
   doc={'schema':1,'mirrors':{},'defaults':{'download_dir':r'{base}\logs\setup\dl','temp_dir':r'{base}\logs\setup\tmp'},'components':[]}
   for source in original['components']:
    if source['tier'] not in ('required','default','manual'):continue
    c=copy.deepcopy(source);cid=c['id'];c['requires']=[]
    c['detect']=c['verify']={'kind':'file','paths':['{base}/ready/'+cid]}
    c['size_bytes']=len(DATA) if cid=='node' else 0
    if c['tier']=='manual':c['manual']=True;c['install']={'steps':[{'kind':'manual','steps':['通过设置手动完成']}]}
    elif cid=='node':
     urls=[origin+('/bad/' if mode['bad'] else '/good/')+'pkg.bin',origin+'/good/pkg.bin']
     c['install']={'steps':[{'kind':'download','urls':urls,'sha256':SHA,'size':(10**17 if mode['huge'] else len(DATA)),'dest':r'{base}\ready\node','layout':'file'}]}
    else:c['install']={'steps':[{'kind':'write_file','path':'{base}/ready/'+cid,'content':'ok'}]}
    doc['components'].append(c)
   payload=json.dumps(doc).encode();self.send_response(200);self.send_header('Content-Length',len(payload));self.end_headers()
   if not head:self.wfile.write(payload)
   return
  if self.path not in ('/good/pkg.bin','/bad/pkg.bin'):self.send_error(404);return
  start=int(self.headers.get('Range','bytes=0-').split('=')[1].split('-')[0]);requests.append({'path':self.path,'start':start,'at':time.time()})
  self.send_response(206 if start else 200);self.send_header('Accept-Ranges','bytes');self.send_header('Content-Length',len(DATA)-start)
  if start:self.send_header('Content-Range',f'bytes {start}-{len(DATA)-1}/{len(DATA)}')
  self.end_headers()
  if head:return
  try:
   for pos in range(start,len(DATA),65536):
    block=DATA[pos:pos+65536]
    if self.path.startswith('/bad/'):block=b'X'*len(block)
    self.wfile.write(block);self.wfile.flush();time.sleep(len(block)/mode['rate'])
  except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);origin=f'http://127.0.0.1:{server.server_port}'
threading.Thread(target=server.serve_forever,daemon=True).start()
env={**os.environ,'KAIROS_SETUP_TEST':'1','KAIROS_SETUP_ORIGIN':origin}
def launch(name,extra=()):
 base=WORK/name;base.mkdir(exist_ok=True);log=open(OUT/(name+'.log'),'ab')
 args=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/bootstrap.ps1'),'-Base',str(base),'-Workspace',str(base),'-Bots',str(base/'bots'),*extra]
 return subprocess.Popen(args,env=env,cwd=ROOT,stdout=log,stderr=log),base
def snap(base):
 try:return json.loads((base/'logs/setup/progress.json').read_text(encoding='utf-8-sig'))
 except (FileNotFoundError,json.JSONDecodeError):return {}
def await_bytes(p,base,minimum=1,timeout=15):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  if p.poll() is not None:raise AssertionError(f'exited {p.returncode}, inspect {base.name}.log')
  s=snap(base);i=next((i for i in s.get('items',[]) if i['id']=='node'),{})
  if i.get('done',0)>=minimum and i.get('speed',0)>0:return i
  time.sleep(.1)
 raise AssertionError('bytes did not grow within deadline')
def resume(name,base):
 ids=[i['id'] for i in snap(base)['items'] if i['id'] in snap(base)['selection'] and i['state'] not in ('done','manual','skipped')]
 (base/'logs/setup/pause').unlink(missing_ok=True)
 return launch(name,('-Component',','.join(ids)))[0]
report={}
try:
 p,base=launch('pause');first=await_bytes(p,base);second=await_bytes(p,base,first['done']+1)
 assert second['done']>first['done'];t=time.monotonic();(base/'logs/setup/pause').write_text('pause')
 assert p.wait(timeout=10)==75;pause_duration=time.monotonic()-t;paused=snap(base)
 cache=next((base/'logs/setup/dl').glob('node-*'));size=cache.stat().st_size
 (base/'logs/setup/pause').unlink()
 repair,_=launch('pause',('-Component','bots-launchers'));assert repair.wait(timeout=15)==0
 assert 'node' in snap(base)['selection'];assert cache.stat().st_size==size
 report['single_repair_keeps_interrupted_queue']=True
 p=resume('pause',base);assert p.wait(timeout=45)==0
 assert any(r['start']==size for r in requests),('no Range resume',size,requests)
 assert hashlib.sha256((base/'ready/node').read_bytes()).hexdigest()==SHA
 assert all(i['state']=='done' for i in snap(base)['items'] if i['tier']=='required')
 assert all(i['state']=='manual' and i['total']==0 for i in snap(base)['items'] if i['tier']=='manual')
 assert all(next(i for i in snap(base)['items'] if i['id']==id)['state']=='done' for id in ['rift','breeze'])
 assert 'ollama' not in snap(base)['selection'];assert 'ollama-service' not in snap(base)['selection'];assert next(i for i in snap(base)['items'] if i['id']=='ollama-service')['state']=='pending'
 report['defaults']={'rift_done':True,'breeze_done':True,'ollama_unselected':True,'service_deferred_without_client':True}
 # Offline recovery reuses exactly the validated package, with no package HTTP request.
 before=len(requests);(base/'ready/node').unlink();offline,_=launch('pause',('-Component','node','-Offline'));assert offline.wait(timeout=15)==0
 assert len(requests)==before;assert hashlib.sha256((base/'ready/node').read_bytes()).hexdigest()==SHA;report['offline_cache_no_download']=True
 report['pause']={'stop_seconds' :round(pause_duration,3),'bytes':size,'speed':second['speed'],'samples':[first['done'],second['done']],'range_resume':True,'required_done':8,'manual_outside_queue':4}
 p,base=launch('killed');i=await_bytes(p,base);subprocess.run(['taskkill','/PID',str(p.pid),'/T','/F'],capture_output=True);p.wait(timeout=10)
 cache=next((base/'logs/setup/dl').glob('node-*'));size=cache.stat().st_size;p=resume('killed',base);assert p.wait(timeout=45)==0
 assert any(r['start']==size for r in requests);report['killed']={'checkpoint_bytes':i['done'],'file_bytes':size,'range_resume':True}
 mode['bad']=True;mode['rate']=8*1024*1024
 p,base=launch('hash');assert p.wait(timeout=20)==0;assert hashlib.sha256((base/'ready/node').read_bytes()).hexdigest()==SHA
 assert any(r['path'].startswith('/bad/') for r in requests) and any(r['path'].startswith('/good/') for r in requests)
 report['sha256_mirror_fallback']=True
 mode['huge']=True;before=len(requests);p,base=launch('space');assert p.wait(timeout=15)==2
 assert not any(r for r in requests[before:] if r['path'].endswith('pkg.bin'));report['insufficient_disk_no_request']=True
 assert next(i for i in snap(base)['items'] if i['id']=='node')['label']=='磁盘空间不足，请腾出空间后重试'
 # -Check is read-only with respect to live/legacy progress and never fetches a package.
 mode['huge']=False;before=len(requests);old=(base/'logs/setup/progress.json').read_bytes();p,_=launch('space',('-Check',));assert p.wait(timeout=15)==2
 assert not requests[before:];assert (base/'logs/setup/progress.json').read_bytes()==old;report['check_keeps_checkpoint']=True
 # A release-shaped tools directory ignores the two fake-source variables entirely.
 release=WORK/'release';(release/'tools').mkdir(parents=True)
 for file in (ROOT/'tools').iterdir():
  if file.suffix in ('.ps1','.json'):shutil.copy2(file,release/'tools'/file.name)
 before=len(requests)
 isolated=WORK/'release-workspace'
 result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(release/'tools/bootstrap.ps1'),'-Base',str(isolated),'-Workspace',str(isolated),'-Component','webview2','-Check'],env=env,cwd=release,capture_output=True,timeout=15)
 (OUT/'release-check.txt').write_bytes(result.stdout+result.stderr)
 assert result.returncode in (0,2);assert len(requests)==before;assert not (isolated/'logs/setup/test-components.json').exists();report['release_ignores_test_environment']=True
 # Actual C++ timer and worker, not a browser mock: no welcome download, opt-out, 8s repair gate.
 native=ROOT/'win32/build/setup_manager.test.exe'
 if native.exists():
  report['native_startup']={}
  for scenario in ['welcome','disabled','repair','future','optional']:
   result=subprocess.run([str(native),scenario],env=env,cwd=ROOT,capture_output=True,timeout=35)
   (OUT/('native-'+scenario+'.txt')).write_bytes(result.stdout+result.stderr)
   assert result.returncode==0,(scenario,result.returncode);report['native_startup'][scenario]=True
 # Exercise the actual native welcome/workspace controls and real WebView2 handoff.
 if os.environ.get('KAIROS_NATIVE_WINDOW_TEST')=='1':
  mode['rate']=1024*1024;mode['bad']=False
  result=subprocess.run(['python',str(ROOT/'acceptance/fixtures/native-window.py')],env=env,cwd=ROOT,capture_output=True,timeout=100)
  (OUT/'native-window.txt').write_bytes(result.stdout+result.stderr)
  assert result.returncode==0,result.returncode
  assert json.loads((OUT/'native-window.json').read_text(encoding='utf-8'))['real_webview_html_handoff']
  report['native_window_handoff']=True
 report['requests']=requests
finally:
 if sys.exc_info()[0] is not None:
  for log in sorted(OUT.glob('*.log')):
   print('FAKE-SOURCE FAILURE LOG:',log.name,flush=True)
   print(log.read_bytes()[-12000:].decode('utf-8',errors='replace'),flush=True)
 server.shutdown();(OUT/'fake-source.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
