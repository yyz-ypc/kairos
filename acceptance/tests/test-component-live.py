"""Live download only: baseline, progress, pause, Range resume, cancel; no install."""
import json,os,pathlib,subprocess,time,uuid,urllib.parse,sys
ROOT=pathlib.Path(__file__).resolve().parents[2];OUT=ROOT/'evidence/component-supply'
doc=json.loads((ROOT/'tools/components.json').read_text(encoding='utf-8-sig'));c=next(c for c in doc['components'] if c['id']==(sys.argv[1] if len(sys.argv)>1 else 'ollama'))
dest=ROOT/'.tmp/component-live'/uuid.uuid4().hex;dest.mkdir(parents=True)
report={'component_id':c['id'],'probes':[],'installed':False,'rate_limited':False}
def curl(url,*args):
 p=subprocess.run(['curl.exe','-L','-f','-sS','--connect-timeout','5','--max-time','10',*args,url],capture_output=True,text=True)
 return p
def snapshot():
 try:return json.loads((dest/'progress.json').read_text(encoding='utf-8-sig'))
 except (FileNotFoundError,json.JSONDecodeError):return {}
def launch():
 log=open(OUT/('live-transfer-'+c['id']+'.log'),'ab')
 return subprocess.Popen(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'acceptance/fixtures/live-download.ps1'),'-Destination',str(dest),'-Id',c['id'],'-Url',url],cwd=ROOT,stdout=log,stderr=log)
def sample(p,minimum=0):
 begin=time.monotonic()
 while time.monotonic()-begin<10:
  s=snapshot()
  if s.get('done',0)>minimum and s.get('speed',0)>0 and s.get('eta',0)>0:return s,time.monotonic()-begin
  if p.poll() is not None:raise RuntimeError('download ended before progress sample: '+str(p.returncode))
  time.sleep(.1)
 raise RuntimeError('no increasing bytes/speed/ETA within 10 seconds')
try:
 url=''
 for prefix in doc['mirrors']['github']:
  candidate=prefix+c['source']['url'];probe=curl(candidate,'-I','-o','NUL','-w','%{http_code}')
  probe_record={'domain':urllib.parse.urlsplit(candidate).hostname,'exit':probe.returncode,'http':probe.stdout}
  report['probes'].append(probe_record)
  if probe.returncode==0:
   transfer=curl(candidate,'-r','0-8388607','-o','NUL','-w','%{speed_download} %{size_download}')
   try:probe_record['speed']=float(transfer.stdout.split()[0])
   except (ValueError,IndexError):probe_record['speed']=0
   if probe_record['speed']>report.get('best_probe_speed',0):url=candidate;report['best_probe_speed']=probe_record['speed']
 if not url:raise RuntimeError('no live mirror reachable')
 report['mirror_domain']=urllib.parse.urlsplit(url).hostname
 baseline=curl(url,'-r','0-67108863','-o','NUL','-w','%{speed_download} %{size_download}')
 report['baseline_exit']=baseline.returncode;report['baseline_bytes_per_second']=float(baseline.stdout.split()[0]);report['baseline_bytes']=int(baseline.stdout.split()[1])
 p=launch();s,elapsed=sample(p);report['first_sample_seconds']=round(elapsed,3);report['speed_bytes_per_second']=s['speed'];report['eta_seconds']=s['eta']
 t=time.monotonic();(dest/'pause').write_text('pause');assert p.wait(timeout=10)==75
 report['pause_seconds']=round(time.monotonic()-t,3);report['paused_bytes']=(dest/'package.partial').stat().st_size
 (dest/'pause').unlink();p=launch();s,elapsed=sample(p,report['paused_bytes']);report['resumed_bytes']=s['done'];report['resumed_speed_bytes_per_second']=s['speed'];report['range_resume']=s['done']>report['paused_bytes']
 (dest/'pause').write_text('pause');assert p.wait(timeout=10)==75
 s=snapshot();s['selection']=[];s['state']='idle';(dest/'progress.json').write_text(json.dumps(s),encoding='utf-8')
 report['cancelled_queue_empty']=True;report['retained_bytes']=(dest/'package.partial').stat().st_size
 report['baseline_ratio']=report['speed_bytes_per_second']/report['baseline_bytes_per_second'] if report['baseline_bytes_per_second']>0 else 0
 report['meets_60_percent_of_same_source_baseline']=report['baseline_ratio']>=.6
except Exception as exc:
 report['error']=str(exc)
 if 'p' in globals() and p.poll() is None:
  (dest/'pause').write_text('pause')
  try:p.wait(timeout=10)
  except subprocess.TimeoutExpired:subprocess.run(['taskkill','/PID',str(p.pid),'/T','/F'],capture_output=True)
finally:
 (OUT/('live-network-'+c['id']+'.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
 (OUT/'live-network.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2));raise SystemExit(1 if report.get('error') else 0)
