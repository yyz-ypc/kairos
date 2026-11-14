import json,pathlib,subprocess,time,uuid,urllib.parse
ROOT=pathlib.Path(__file__).resolve().parents[2];OUT=ROOT/'evidence/component-supply'
prior=json.loads((OUT/'live-network-ffmpeg.json').read_text());doc=json.loads((ROOT/'tools/components.json').read_text(encoding='utf-8-sig'));c=next(x for x in doc['components'] if x['id']=='ffmpeg')
prefix=next(x for x in doc['mirrors']['github'] if urllib.parse.urlsplit(x or c['source']['url']).hostname==prior['mirror_domain']);url=prefix+c['source']['url']
dest=ROOT/'.tmp/component-live'/uuid.uuid4().hex;dest.mkdir(parents=True)
report={'component_id':'ffmpeg','mirror_domain':prior['mirror_domain'],'samples':[],'installed':False,'cancelled':False}
def snap():
 try:return json.loads((dest/'progress.json').read_text(encoding='utf-8-sig'))
 except (FileNotFoundError,json.JSONDecodeError):return {}
def transfer(seconds):
 previous_run=snap().get('run_id')
 p=subprocess.Popen(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'acceptance/fixtures/live-download.ps1'),'-Destination',str(dest),'-Id','ffmpeg','-Url',url],stdout=open(OUT/'live-warm.log','ab'),stderr=subprocess.STDOUT,cwd=ROOT)
 start=time.monotonic();seen=set()
 while time.monotonic()-start<seconds and p.poll() is None:
  s=snap();key=(s.get('done'),s.get('speed'))
  if s.get('run_id')!=previous_run and s.get('speed',0)>0 and key not in seen:report['samples'].append({'elapsed':round(time.monotonic()-start,3),'done':s['done'],'speed':s['speed'],'eta':s['eta']});seen.add(key)
  time.sleep(.1)
 stop=time.monotonic();(dest/'pause').write_text('pause');assert p.wait(timeout=10)==75
 return round(time.monotonic()-stop,3),(dest/'package.partial').stat().st_size
try:
 report['pause_seconds'],report['paused_bytes']=transfer(8)
 (dest/'pause').unlink();before=len(report['samples']);report['resume_pause_seconds'],report['retained_bytes']=transfer(8)
 report['range_resume']=any(s['done']>report['paused_bytes'] for s in report['samples'][before:]);s=snap();s['selection']=[];s['state']='idle';(dest/'progress.json').write_text(json.dumps(s));report['cancelled']=True
 # Compare the same mirror and request shape for the same wall-clock interval.
 raw=subprocess.run(['curl.exe','-L','-f','-sS','-C','-','--connect-timeout','5','--max-time','8','-o',str(dest/'raw-baseline.partial'),'-w','%{speed_download} %{size_download}',url],capture_output=True,text=True)
 report['raw_full_request_baseline']=float(raw.stdout.split()[0]);report['peak_speed']=max(s['speed'] for s in report['samples']);report['same_request_baseline_ratio']=report['peak_speed']/report['raw_full_request_baseline'];report['meets_same_request_60_percent']=report['same_request_baseline_ratio']>=.6
except Exception as e:report['error']=str(e)
finally:(OUT/'live-network-warm.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
