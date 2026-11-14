"""Exercise only an isolated copy of the test EXE; never attach to an existing app."""
import ctypes,ctypes.wintypes as w,json,os,pathlib,re,shutil,subprocess,sys,time,uuid
ROOT=pathlib.Path(__file__).resolve().parents[2]
u=ctypes.windll.user32
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetDlgItem.argtypes=[w.HWND,ctypes.c_int];u.GetDlgItem.restype=w.HWND
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=ctypes.c_ssize_t
u.GetWindowThreadProcessId.argtypes=[w.HWND,ctypes.POINTER(w.DWORD)]
u.ShowWindow.argtypes=[w.HWND,ctypes.c_int]
u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
cls=re.search(r'#define BRAND_WINDOW_CLASS L"([^"]+)"',(ROOT/'win32/src/brand.h').read_text(encoding='utf-8-sig'))[1]
if u.FindWindowW(cls,None):raise RuntimeError('已有同名工作台窗口；不会操作用户窗口，请单独运行此测试')
area=ROOT/'.tmp/native-window'/uuid.uuid4().hex;program=area/'program';base=area/'workspace';state=area/'state'
for p in [program/'tools',program/'acceptance/fixtures',program/'art',base,state]:p.mkdir(parents=True,exist_ok=True)
for name in ['bootstrap.ps1','common.ps1','progress.ps1','autostart.ps1','components.json']:shutil.copy2(ROOT/'tools'/name,program/'tools'/name)
for name in ['Kairos-setup-review.exe','WebView2Loader.dll']:shutil.copy2(ROOT/'bin'/name,program/name)
for name in ['hero.png','avatar.png']:shutil.copy2(ROOT/'art'/name,program/'art'/name)
shutil.copy2(ROOT/'acceptance/fixtures/setup-source.ps1',program/'acceptance/fixtures/setup-source.ps1')
env={**os.environ,'KAIROS_STATE_DIR':str(state),'KAIROS_WORKSPACE':str(base),'KAIROS_BASE':str(base),'KAIROS_SIDECAR':str(program/'sidecar'),'KAIROS_UI_FALLBACK':'1','KAIROS_UPDATE_RESTART':'0','NO_PROXY':'127.0.0.1,localhost','no_proxy':'127.0.0.1,localhost'}
for name in ['WEBVIEW2_USER_DATA_FOLDER','WEBVIEW2_BROWSER_EXECUTABLE_FOLDER','WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS']:env.pop(name,None)
settings=state/'settings.json';progress=base/'logs/setup/progress.json'
report={};process=None
def read(path):
 try:return json.loads(path.read_text(encoding='utf-8-sig'))
 except (FileNotFoundError,json.JSONDecodeError):return {}
def wait(predicate,timeout=20):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  if predicate():return
  if process and process.poll() is not None:raise RuntimeError('test app exited '+str(process.returncode))
  time.sleep(.1)
 raise RuntimeError('native window test timed out')
def launch():
 global process
 if u.FindWindowW(cls,None):raise RuntimeError('窗口仍在，拒绝复用未知进程')
 info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
 process=subprocess.Popen([str(program/'Kairos-setup-review.exe')],cwd=program,env=env,startupinfo=info)
 found=[]
 def locate():
  handle=u.FindWindowW(cls,None)
  if not handle:return False
  pid=w.DWORD();u.GetWindowThreadProcessId(handle,ctypes.byref(pid))
  if pid.value!=process.pid:raise RuntimeError('拒绝操作非测试窗口')
  found.append(handle);u.ShowWindow(handle,0);return True
 wait(locate);handle=found[-1];wait(lambda:bool(u.GetDlgItem(handle,62001)));return handle
def kill():
 if process and process.poll() is None:subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],capture_output=True);process.wait(timeout=10)
def click(handle,id):u.SendMessageW(handle,0x111,id,0)
def label(handle,id):
 text=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(u.GetDlgItem(handle,id),text,1024);return text.value
try:
 handle=launch();time.sleep(10);assert read(settings).get('wizard_step','0')=='0';assert not progress.exists();report['welcome_10_seconds_no_download']=True
 click(handle,62001);wait(lambda:read(settings).get('wizard_step')=='1');time.sleep(2);assert not progress.exists();report['workspace_no_download']=True
 click(handle,62001);wait(lambda:any(i['id']=='webview2' and i['state']=='done' for i in read(progress).get('items',[])))
 wait(lambda:label(handle,62001)!='暂停');assert read(progress)['selection']==['webview2'];report['runtime_only_before_html']=True
 click(handle,62001)
 wait(lambda:any(i['id']=='node' and i.get('done',0)>0 for i in read(progress).get('items',[])),timeout=30)
 assert not u.GetDlgItem(handle,62001);report['real_webview_html_handoff']=True
 pause=base/'logs/setup/pause';pause.write_text('pause');wait(lambda:read(progress).get('state')=='paused')
 cache=next((base/'logs/setup/dl').glob('node-*'));size=cache.stat().st_size;report['bytes_before_restart']=size;kill()
 handle=launch();wait(lambda:any(i['id']=='node' and i.get('done',0)>size for i in read(progress).get('items',[])),timeout=20)
 report['app_kill_restart_resumes_file']=True
 pause.write_text('pause');wait(lambda:read(progress).get('state')=='paused')
 report['wizard_step_preserved']=read(settings).get('wizard_step')=='2';report['installed_real_components']=False
finally:
 kill();(ROOT/'evidence/component-supply/native-window.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
