import fs from 'node:fs';
import {createHash} from 'node:crypto';
import path from 'node:path';
import vm from 'node:vm';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
const root=path.resolve('.');
const testPath=path.join(root,'win32/tests/webui.test.cjs');
const helpers=fs.readFileSync(testPath,'utf8').split("test('all eight pages")[0];
const sandbox=vm.createContext({require:createRequire(testPath),__dirname:path.dirname(testPath),console});
const fixture=JSON.parse(JSON.stringify(vm.runInContext(helpers+';fixture().state',sandbox)));
const tmp=path.join(root,'.tmp/matrix');fs.mkdirSync(tmp,{recursive:true});
fs.copyFileSync('win32/web/index.html',path.join(tmp,'index.html'));
// The same short asset paths are served from native RCDATA and from this isolated fixture.
for(const sub of ['styles','character']){fs.mkdirSync(path.join(tmp,'art',sub),{recursive:true});fs.cpSync(path.join(root,'art',sub),path.join(tmp,'art',sub),{recursive:true})}
const target=await(await fetch('http://127.0.0.1:9342/json/new?about:blank',{method:'PUT'})).json();
if(!target.webSocketDebuggerUrl)throw Error('No isolated matrix browser page');
const socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((ok,no)=>{socket.onopen=ok;socket.onerror=no});
let id=0;const pending=new Map(),errors=[];
socket.onmessage=e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);if(p){pending.delete(m.id);m.error?p.no(Error(JSON.stringify(m.error))):p.ok(m.result)}}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);else if(m.method==='Log.entryAdded'&&m.params.entry.level==='error')errors.push(m.params.entry)};
function call(method,params={}){return new Promise((ok,no)=>{const n=++id;pending.set(n,{ok,no});socket.send(JSON.stringify({id:n,method,params}));setTimeout(()=>{if(pending.has(n)){pending.delete(n);no(Error('CDP timeout: '+method))}},10000).unref()})}
async function evaluate(expression){const v=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(v.exceptionDetails)throw Error(JSON.stringify(v.exceptionDetails));return v.result.value}
await call('Page.enable');await call('Runtime.enable');await call('Log.enable');
await call('Page.addScriptToEvaluateOnNewDocument',{source:'window.chrome={webview:{postMessage(){},addEventListener(){}}};'});
await call('Page.navigate',{url:pathToFileURL(path.join(tmp,'index.html')).href});
for(let n=0;n<100;n++){if(await evaluate('typeof render === "function"'))break;await new Promise(ok=>setTimeout(ok,50))}
const cases=[],shots=path.join(root,'evidence/screens');fs.mkdirSync(shots,{recursive:true});
for(const [width,height] of [[1400,900],[1152,598],[760,600]]){
 await call('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:1,mobile:false});
 for(const style of ['seer','array','barrier','tower','folio'])for(const theme of ['nightfall','daylight','boundary','moon','suit'])for(const tab of [0,5,7,3,4,6]){
  const state={...fixture,style,theme,tab,wizardComplete:true,tide:{reachable:true,managed:true,state:{accounts:[],native:{},networks:[]}}};
  await evaluate(`document.activeElement?.blur();settingsSec=0;force=true;render(${JSON.stringify(state)});`);
  const report=await evaluate(`new Promise(ok=>requestAnimationFrame(()=>requestAnimationFrame(()=>{fitScreens();setTimeout(()=>{
   const outside=[...document.querySelectorAll('h1,h2,button,input,label,.row,.feat,.wizard-controls')].filter(el=>{const q=el.getBoundingClientRect();const c=getComputedStyle(el);return q.width>0&&q.height>0&&c.visibility!=='hidden'&&(q.right>innerWidth+2||q.left < -2||q.bottom>innerHeight+2)}).slice(0,8).map(el=>({tag:el.tagName,text:el.textContent.slice(0,40),rect:JSON.stringify(el.getBoundingClientRect())}));
   ok({bodyX:document.documentElement.scrollWidth>innerWidth+1,bodyY:document.documentElement.scrollHeight>innerHeight+1,outside});
  },360)})))`);
  cases.push({width,height,style,theme,tab,...report});
  if(width===1400&&[0,4,6].includes(tab)){const dir=path.join(root,'evidence/matrix',style+'-'+theme);fs.mkdirSync(dir,{recursive:true});const snap=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(dir,tab+'.png'),Buffer.from(snap.data,'base64'))}
  if(width===1400&&theme==='nightfall'&&tab===0){const snap=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(shots,style+'-today.png'),Buffer.from(snap.data,'base64'))}
 }
 console.log('Matrix viewport complete: '+width+'x'+height);
}
// All five onboarding states in every style at the smallest requested size.
for(const style of ['seer','array','barrier','tower','folio'])for(let wizardStep=0;wizardStep<6;wizardStep++){
 await evaluate(`force=true;render(${JSON.stringify({...fixture,style,wizardComplete:false,wizardStep,tide:{reachable:true,managed:true,state:{accounts:[],native:{},networks:[]}}})});`);
 await new Promise(ok=>setTimeout(ok,360));
 const report=await evaluate(`({bodyX:document.documentElement.scrollWidth>innerWidth+1,bodyY:document.documentElement.scrollHeight>innerHeight+1,controls:document.querySelector('.wizard-controls').getBoundingClientRect().bottom<=innerHeight})`);
 cases.push({width:760,height:600,style,wizardStep,...report});
 if(style==='barrier'){const snap=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(shots,'onboarding-'+wizardStep+'.png'),Buffer.from(snap.data,'base64'))}
}
// Component views use actual catalog metadata and deterministic live transfer samples.
const components=JSON.parse(fs.readFileSync('tools/components.json','utf8')).components.map(c=>({...c,selected:c.default_install}));
const setup={state:'downloading',busy:true,pct:42,done:18600000,total:44200000,speed:1200000,eta:22,phase:'下载',auto_repair:true,components,items:components.map(c=>({id:c.id,state:c.tier==='manual'?'manual':c.id==='node'?'downloading':'pending',label:c.id==='node'?'下载':'检查',done:c.id==='node'?18600000:0,total:c.size_bytes,speed:c.id==='node'?1200000:0}))};
for(const [width,height] of [[1400,900],[1152,598],[760,600]]){
 await call('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:1,mobile:false});
 for(const style of ['seer','array','barrier','tower','folio'])for(const route of ['wizard','settings','diagnostics','manual-memory','manual-bili','manual-qq']){
  const manualId={'manual-memory':'memory-plugin','manual-bili':'bili-login','manual-qq':'qq-login'}[route];
  const s={...fixture,style,theme:'nightfall',setup:manualId?{...setup,items:setup.items.map(i=>({...i,state:components.find(c=>c.id===i.id).tier==='required'?'done':i.state}))}:setup,tab:route==='diagnostics'?4:6,wizardComplete:route!=='wizard'&&!manualId,wizardStep:2,wizardManual:manualId,bconfig:manualId==='bili-login'?Array.from({length:24},(_,row)=>({row,label:'账号设置 '+row,hint:'按说明填写；密钥保存在本机。',kind:row===0?5:0,value:''})):fixture.bconfig};
  await evaluate(`document.activeElement?.blur();settingsSec=4;screenChoices.set('diagnostics',0);force=true;render(${JSON.stringify(s)});`);
  await new Promise(ok=>setTimeout(ok,400));
  const report=await evaluate(`(()=>{fitScreens();const outside=[...document.querySelectorAll('#page button,#page .setup-row')].filter(el=>{const r=el.getBoundingClientRect();return r.width>0&&r.height>0&&(r.left< -2||r.right>innerWidth+2||r.bottom>innerHeight+2)}).slice(0,6).map(el=>el.textContent.slice(0,30));return{bodyX:document.documentElement.scrollWidth>innerWidth+1,bodyY:document.documentElement.scrollHeight>innerHeight+1,outside}})()`);
  cases.push({width,height,style,route,...report});
 }
}
const tooltip=[];
await call('Emulation.setDeviceMetricsOverride',{width:1152,height:700,deviceScaleFactor:1,mobile:false});
for(const route of ['wizard','settings','diagnostics','manual-memory','manual-bili','manual-qq']){
 await evaluate(`document.activeElement?.blur();settingsSec=4;screenChoices.set('diagnostics',0);force=true;render(${JSON.stringify({...fixture,style:'seer',theme:'nightfall',setup,tab:4,wizardComplete:true})});`);
 if(route==='settings')await evaluate(`force=true;render(${JSON.stringify({...fixture,style:'seer',theme:'nightfall',setup,tab:6,wizardComplete:true})});`);
 if(route==='wizard')await evaluate(`force=true;render(${JSON.stringify({...fixture,style:'seer',theme:'nightfall',setup,wizardStep:2,wizardComplete:false})});`);
 await new Promise(ok=>setTimeout(ok,400));
 // Walk the actual focus order through Tab, including the information buttons.
 await evaluate(`document.activeElement?.blur();document.body.tabIndex=-1;document.body.focus()`);
 let tabReachable=false;
 for(let n=0;n<100;n++){
  await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Tab',code:'Tab',windowsVirtualKeyCode:9});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Tab',code:'Tab',windowsVirtualKeyCode:9});
  if(await evaluate(`!!document.activeElement?.dataset?.setupInfo`)){tabReachable=true;break}
 }
 const report=await evaluate(`(()=>{const b=document.activeElement;b.style.position='fixed';b.style.right='8px';b.style.top='150px';b.style.zIndex='4';showSetupTip(b);const r=document.getElementById('setup-tooltip')?.getBoundingClientRect();return{right:r?.right,left:r?.left,bottom:r?.bottom,inViewport:!!r&&r.left>=0&&r.right<=innerWidth&&r.top>=0&&r.bottom<=innerHeight}})()`);
 tooltip.push({route,tabReachable,...report});
 const snap=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join('evidence/component-supply',route+'-tooltip.png'),Buffer.from(snap.data,'base64'));
 await evaluate(`hideSetupTip()`);
}
await evaluate(`settingsSec=4;screenChoices.set('settings-components',3);force=true;render(${JSON.stringify({...fixture,style:'seer',setup,tab:6,wizardComplete:true})});`);
await new Promise(ok=>setTimeout(ok,400));
await evaluate(`const disclosure=document.querySelector('.setup-manual');if(disclosure)disclosure.open=true;fitScreens()`);
const manual=await evaluate(`(()=>{const p=document.querySelector('#screen-settings-components-3');return{hasInstructions:p.textContent.includes('LivingMemory'),noBytes:!(p.textContent.includes('MB/s')||p.textContent.includes('剩余')),noProgressbar:!p.querySelector('[role=progressbar]')}})()`);
const snap=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync('evidence/component-supply/manual-instructions.png',Buffer.from(snap.data,'base64'));
// Touch tap stays open after focus + synthesized click; a second/outside tap dismisses it.
await call('Emulation.setTouchEmulationEnabled',{enabled:true});
const touchPoint=await evaluate(`(()=>{const b=[...document.querySelectorAll('[data-setup-info]')].find(b=>{const r=b.getBoundingClientRect();return r.width>0&&r.height>0&&!b.closest('[hidden]')});const r=b.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`);
async function tap(x,y){await call('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y,id:0}]});await call('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await new Promise(ok=>setTimeout(ok,350))}
await tap(touchPoint.x,touchPoint.y);const touchOpened=await evaluate(`!!document.getElementById('setup-tooltip')`);
await tap(170,90);const touchDismissed=await evaluate(`!document.getElementById('setup-tooltip')`);
await call('Emulation.setTouchEmulationEnabled',{enabled:false});
const touch={opened:touchOpened,dismissed:touchDismissed};if(!touchOpened||!touchDismissed)errors.push('touch tooltip failed');
fs.writeFileSync('evidence/component-supply/tooltip.json',JSON.stringify({tooltip,manual,touch},null,2));
if(tooltip.some(t=>!t.inViewport||!t.tabReachable)||!manual.hasInstructions||!manual.noBytes||!manual.noProgressbar)errors.push('tooltip/manual acceptance failed');
const failures=cases.filter(c=>c.bodyX||c.bodyY||c.outside?.length||c.controls===false);
fs.writeFileSync('evidence/render-matrix.json',JSON.stringify({inputSha256:createHash('sha256').update(fs.readFileSync('win32/web/index.html')).digest('hex'),cases:cases.length,failures,errors,results:cases},null,2));
console.log(JSON.stringify({cases:cases.length,failures:failures.length,consoleErrors:errors.length}));
socket.close();await fetch('http://127.0.0.1:9342/json/close/'+target.id);process.exitCode=failures.length||errors.length?1:0;
