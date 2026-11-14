import fs from 'node:fs';import path from 'node:path';import {spawn,execFileSync} from 'node:child_process';import {performance} from 'node:perf_hooks';import net from 'node:net';import vm from 'node:vm';import {createRequire} from 'node:module';
const root=path.resolve('.'),out=path.join(root,'evidence/release'),temp=path.join(root,'.tmp','webview-bench-'+Date.now());fs.mkdirSync(out,{recursive:true});fs.mkdirSync(temp,{recursive:true});fs.mkdirSync(path.join(temp,'empty-sidecar'),{recursive:true});
const server=net.createServer();await new Promise(r=>server.listen(0,'127.0.0.1',r));const port=server.address().port;await new Promise(r=>server.close(r));
const testPath=path.join(root,'win32/tests/webui.test.cjs'),helpers=fs.readFileSync(testPath,'utf8').split("test('all eight pages")[0],sandbox=vm.createContext({require:createRequire(testPath),__dirname:path.dirname(testPath),console});const fixture=JSON.parse(JSON.stringify(vm.runInContext(helpers+';fixture().state',sandbox)));
const begin=performance.now();const app=spawn(path.join(root,'bin/Kairos.exe'),['--edge-webview-switches=--remote-debugging-port='+port],{cwd:root,windowsHide:true,stdio:'ignore',env:{...process.env,KAIROS_BENCH_LOG:path.join(temp,'startup.log'),KAIROS_STATE_DIR:path.join(temp,'state'),KAIROS_WORKSPACE:path.join(temp,'workspace'),KAIROS_SIDECAR:path.join(temp,'empty-sidecar'),KAIROS_TIDE_PORT:'25389',WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS:'--remote-debugging-port='+port}});
let socket,id=0,pending=new Map(),navigations=0;
async function evaluate(expression){const v=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(v.exceptionDetails)throw Error(JSON.stringify(v.exceptionDetails));return v.result.value}
function call(method,params={}){return new Promise((ok,no)=>{const n=++id;pending.set(n,{ok,no});socket.send(JSON.stringify({id:n,method,params}));setTimeout(()=>{if(pending.has(n)){pending.delete(n);no(Error('Timeout '+method))}},15000).unref()})}
function memory(){const ps=`$all=Get-CimInstance Win32_Process;$ids=New-Object 'System.Collections.Generic.HashSet[int]';[void]$ids.Add(${app.pid});do{$changed=$false;foreach($p in $all){if($ids.Contains([int]$p.ParentProcessId)-and -not $ids.Contains([int]$p.ProcessId)){[void]$ids.Add([int]$p.ProcessId);$changed=$true}}}while($changed);$items=@(Get-Process -Id @($ids) -ErrorAction SilentlyContinue|ForEach-Object {@{name=$_.ProcessName;workingSet=$_.WorkingSet64;privateBytes=$_.PrivateMemorySize64}});$items|ConvertTo-Json -Compress`;const raw=execFileSync('powershell.exe',['-NoProfile','-Command',ps],{encoding:'utf8'});const v=JSON.parse(raw);const rows=Array.isArray(v)?v:[v];return {processes:rows,workingSet:rows.reduce((a,b)=>a+b.workingSet,0),privateBytes:rows.reduce((a,b)=>a+b.privateBytes,0)}}
try{
 let target;
 while(performance.now()-begin<15000){try{const tabs=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();target=tabs.find(t=>t.webSocketDebuggerUrl&&t.url.includes('kairos.localhost'))||tabs.find(t=>t.webSocketDebuggerUrl);if(target)break}catch{}await new Promise(r=>setTimeout(r,15))}
 if(!target)throw Error('No isolated WebView2 CDP endpoint');socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((ok,no)=>{socket.onopen=ok;socket.onerror=no});
 socket.onmessage=e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);if(p){pending.delete(m.id);m.error?p.no(Error(JSON.stringify(m.error))):p.ok(m.result)}}else if(m.method==='Page.frameNavigated')navigations++};
 await call('Runtime.enable');await call('Page.enable');
 while(performance.now()-begin<15000){if(await evaluate(`!!document.querySelector('#page h1')&&document.querySelector('#page').textContent.trim().length>20`))break;await new Promise(r=>setTimeout(r,15))}
 const firstScreenMs=performance.now()-begin;
 if(!(await evaluate(`!!document.querySelector('#page h1')`)))throw Error('First screen never rendered');
 await evaluate(`window.__nativeRender=render;render=s=>{if(s.__benchmark)window.__nativeRender(s)};`);
 const baseline={...fixture,__benchmark:true,style:'seer',theme:'nightfall',version:'V1.0.0 · synthetic',wizardComplete:true,tab:0};
 await evaluate(`force=true;render(${JSON.stringify(baseline)});`);await evaluate(`Promise.all([...document.images].map(i=>i.decode().catch(()=>{})))`);await new Promise(r=>setTimeout(r,500));navigations=0;
 const before=memory(),heapBefore=await call('Runtime.getHeapUsage');const times=[];
 for(let cycle=0;cycle<5;cycle++)for(const style of ['seer','array','barrier','tower','folio']){
  const s={...fixture,__benchmark:true,style,theme:['nightfall','daylight','boundary','moon','suit'][cycle],version:'V1.0.0 · synthetic',wizardComplete:true,tab:0};
  const ms=await evaluate(`(async()=>{const t=performance.now();document.activeElement?.blur();force=true;render(${JSON.stringify(s)});await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)));fitScreens();return performance.now()-t})()`);times.push({cycle,style,milliseconds:ms});

 }
 const after=memory(),heapAfter=await call('Runtime.getHeapUsage');
 for(let cycle=0;cycle<5;cycle++)for(const style of ['seer','array','barrier','tower','folio']){
  const s={...fixture,__benchmark:true,style,theme:['nightfall','daylight','boundary','moon','suit'][cycle],version:'V1.0.0 · synthetic',wizardComplete:true,tab:0};
  await evaluate(`(async()=>{force=true;render(${JSON.stringify(s)});await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)));fitScreens()})()`);
 }
 await new Promise(r=>setTimeout(r,500));const repeatAfter=memory();

 const startupTrace=fs.existsSync(path.join(temp,'startup.log'))?fs.readFileSync(path.join(temp,'startup.log'),'utf8').trim().split('\n'):[];
 const webviewBytes=m=>m.processes.filter(p=>p.name==='msedgewebview2').reduce((a,p)=>a+p.privateBytes,0);
 const navigationTiming=await evaluate(`performance.getEntriesByType('navigation').map(e=>e.toJSON())`);
 const paints=await evaluate(`performance.getEntriesByType('paint').map(e=>e.toJSON())`);
 const result={repeatMemoryAfter:repeatAfter,repeatPrivateBytesDelta:repeatAfter.privateBytes-after.privateBytes,repeatWebviewPrivateBytesDelta:webviewBytes(repeatAfter)-webviewBytes(after),paints,navigationTiming,webviewPrivateBytesBefore:webviewBytes(before),webviewPrivateBytesAfter:webviewBytes(after),webviewPrivateBytesDelta:webviewBytes(after)-webviewBytes(before),startupTrace,engine:'actual WebView2 in Kairos.exe',fixture:'isolated synthetic state; no QQ accounts; empty sidecar',profile:'new per-run WebView profile',firstScreenMs:Number(firstScreenMs.toFixed(2)),targetMs:1500,firstScreenPassed:firstScreenMs<1500,navigationsDuringStyleSwitch:navigations,styleSwitchTimes:times,memoryBefore:before,memoryAfter:after,workingSetDelta:after.workingSet-before.workingSet,privateBytesDelta:after.privateBytes-before.privateBytes,jsHeapBefore:heapBefore.usedSize,jsHeapAfter:heapAfter.usedSize,jsHeapDelta:heapAfter.usedSize-heapBefore.usedSize};
 fs.writeFileSync(path.join(out,'performance.json'),JSON.stringify(result,null,2));console.log(JSON.stringify({firstScreenMs:result.firstScreenMs,firstScreenPassed:result.firstScreenPassed,navigations,privateBytesDelta:result.privateBytesDelta,jsHeapDelta:result.jsHeapDelta}));
 if(!result.firstScreenPassed||navigations)process.exitCode=1;
}finally{socket?.close();execFileSync('taskkill.exe',['/PID',String(app.pid),'/T','/F'],{stdio:'ignore'})}
