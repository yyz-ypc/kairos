// The wizard, settings and diagnostics share the same backend-owned queue.
let setupAriaAt=0,setupAriaPct=0,setupLastPhase='',setupTipTimer,setupTipTarget,setupTipPinned=false;
function setupData(){return state?.setup||{}}
function setupCatalog(){return setupData().components||[]}
function setupItem(id){return (setupData().items||[]).find(i=>i.id===id)||{id,state:'pending',label:'等待'}}
function setupReady(){const required=setupCatalog().filter(c=>c.tier==='required');return required.length===8&&required.every(c=>setupItem(c.id).state==='done')}
function setupBytes(n){return (Math.max(0,+n||0)/1048576).toFixed(1)+' MB'}
function setupInfo(c){return `<button class="setup-info" type="button" data-setup-info="${esc(c.id)}" aria-label="${esc(c.name)}，功能说明" aria-describedby="setup-tooltip">ⓘ</button>`}
function setupProgress(){
 const p=setupData(),now=Date.now();if(now-setupAriaAt>=1000){setupAriaAt=now;setupAriaPct=Math.max(0,Math.min(100,+p.pct||0))}
 const pct=Math.max(0,Math.min(100,+p.pct||0)),metrics=p.total>0&&(p.items||[]).some(i=>i.state==='downloading'&&i.total>0)?` · ${setupBytes(p.done)}/${setupBytes(p.total)}${p.speed>0?' · '+setupBytes(p.speed)+'/s · 剩余 '+Math.ceil(p.eta||0)+' 秒':''}`:'';
 return `<div class="setup-summary"><div role="progressbar" aria-label="组件准备总进度" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${setupAriaPct}" class="setup-track"><i style="width:${pct}%"></i></div><p aria-hidden="true">${pct}%${metrics} · ${esc(p.phase||'等待准备组件')}</p><div class="setup-live" aria-live="polite" data-setup-live></div></div>`;
}
function setupAnnounce(){const p=setupData(),el=document.querySelector?.('[data-setup-live]');if(el&&p.phase!==setupLastPhase){setupLastPhase=p.phase||'';el.textContent=setupLastPhase}}
function setupIsBundled(c){return c.source?.kind==='bundled'}
// 随包的潮信只有本地语义：已随包 / 正在初始化 / 已就绪；不转圈下载，也不报网络错。
function setupDetailText(c,st){
 if(setupIsBundled(c))return {pending:'已随包',checking:'检查随包文件',verifying:'校验随包文件',installing:'正在初始化',done:'已就绪',failed:'随包文件未就绪',elevation:'需要管理员',skipped:'本次不初始化'}[st]||'已随包';
 return {pending:'等待',checking:'检查',downloading:'下载',verifying:'校验',installing:'安装',done:'已在位',skipped:'本次不安装',elevation:'需要管理员'}[st]||'等待';
}
function setupRow(c,wizard=false){
 const item=setupItem(c.id),manual=c.tier==='manual',required=c.tier==='required',bundled=setupIsBundled(c);
 const failed=['failed','elevation'].includes(item.state),done=item.state==='done';
 const button=(label,op)=>`<button data-setup="${op}" data-setup-id="${esc(c.id)}">${label}</button>`;
 let actions='',detail='',metrics='';
 if(manual){detail=c.desc;actions=`<button data-setup-settings="${esc(c.id)}"${wizard&&!setupReady()?' disabled':''}>去设置</button>`}
 else {
   // 随包组件只说本地语义（已随包/正在初始化/已就绪）；后端行标签是下载队列用语。
   detail=bundled?setupDetailText(c,item.state):(item.label||setupDetailText(c,item.state));
   if(!bundled&&item.state==='downloading'&&c.size_bytes>0)metrics=` · ${setupBytes(item.done)}/${setupBytes(item.total||c.size_bytes)}${item.speed>0?' · '+setupBytes(item.speed)+'/s':''}`;
   if(failed)actions=bundled?button('修复','retry')+button('看日志','log'):button('重试','retry')+button('换镜像','mirror')+button('看日志','log');
   else if(!wizard)actions=required?button('修复','retry'):button(done?'修复':'安装',done?'retry':'start')+(done&&c.tier==='default'?button('卸载','uninstall'):'');
 }
 const pick=required?'<span aria-label="运行必需，已锁定">🔒</span>':c.tier==='default'?`<input type="checkbox" data-setup-choice="${esc(c.id)}" aria-label="安装 ${esc(c.name)}"${c.selected?' checked':''}>`:'';
 return `<div class="alert-row setup-row" data-setup-row="${esc(c.id)}"><div class="row-t"><b>${pick}${esc(c.name)} ${setupInfo(c)}${c.id==='ollama'?'<small class="dim">可选 · 1.6 GB</small>':''}</b><small>${esc(detail)}${metrics?'<span aria-hidden="true">'+esc(metrics)+'</span>':''}</small></div><div class="row-c">${failed?'<span class="pill bad">'+(item.state==='elevation'?'需要管理员':bundled?'未就绪':'未完成')+'</span>':''}${actions}</div></div>`;
}
function setupGroups(wizard=false){
 const comps=setupCatalog(),rows=tier=>comps.filter(c=>c.tier===tier).map(c=>setupRow(c,wizard)).join('');
 const groups=[['运行必需',rows('required')||empty('正在读取组件清单')],['默认安装',rows('default')]];
 if(!wizard)groups.push(['按需安装',rows('on-demand')]);
 groups.push(['稍后手动',`<details class="setup-manual"><summary>稍后手动 · 按说明完成</summary>${rows('manual')}</details>`]);
 if(!wizard)groups.push(['随包修复',card('补齐随包组件','<p class="dim">优先使用本地 pack；逐文件校验 SHA256。离线时显示下载地址、放置位置和校验方法。</p>'+btn('补齐随包组件','op:repair_bundle',false,state.maintBusy))]);
 return screenMenu(wizard?'wizard-components':'settings-components',groups);
}
function setupControls(){const busy=setupData().busy;return `<div class="actions"><button data-setup="${busy?'pause':'resume'}">${busy?'暂停':'继续准备'}</button><button data-setup="cancel">取消队列</button><button data-setup="log">看日志</button></div>`}
function componentSettings(){return setupProgress()+`<label class="setup-auto"><input type="checkbox" data-setup-auto${setupData().auto_repair!==false?' checked':''}>启动时自动补装缺少的运行必需组件</label>`+setupControls()+setupGroups(false)}
function setupThinProgress(){const p=setupData();return p.busy?`<div class="setup-top" aria-hidden="true"><i style="width:${Math.max(0,Math.min(100,+p.pct||0))}%"></i></div>`:''}
function setupClick(e){
 const b=e.target.closest?.('[data-setup],[data-setup-settings],[data-setup-info]');if(!b){hideSetupTip();return false;}
 if(!('setup' in b.dataset)&&!('setupInfo' in b.dataset)&&!('setupSettings' in b.dataset))return false;
 if(b.dataset.setupInfo){if(setupTipTarget===b&&setupTipPinned)hideSetupTip();else{showSetupTip(b);setupTipPinned=true}return true}
 if(b.dataset.setupSettings){
   // Keep onboarding at its component gate while opening an instruction-only settings sheet.
   if(state.wizardComplete===false){send('setup:settings',{id:b.dataset.setupSettings});return true}
   force=true;
   if(b.dataset.setupSettings==='qq-login'){tideSec=0;send('tab',{index:7});return true}
   if(b.dataset.setupSettings==='bili-login'){cfgSec=0;send('tab',{index:1});send('section',{index:3});return true}
   settingsSec=1;send('tab',{index:6});render(state);return true;
 }
 send('setup:'+b.dataset.setup,{id:b.dataset.setupId||''});return true;
}
function setupChange(e){
 if(e.target.dataset?.setupChoice){send('setup:'+(e.target.checked?'select':'skip-optional'),{id:e.target.dataset.setupChoice});return true}
 if(e.target.hasAttribute?.('data-setup-auto')){send('setup:auto-repair',{id:e.target.checked?'1':'0'});return true}return false;
}
function setupManualContent(id){
 const c=setupCatalog().find(c=>c.id===id);if(!c)return empty('没有找到这项设置');
 if(id==='qq-login'){tideSec=0;return tidePage()}
 if(id==='bili-login'){cfgSec=0;return screenDirect('wizard-manual-bili',configForm())}
 return screenDirect('wizard-manual-provider',card(c.name,`<p>${esc(c.desc)}</p><p class="dim">启动 AstrBot 后，在后台完成插件或模型提供商配置。</p><div class="actions"><button data-service="1" data-value="start">启动 AstrBot</button>${btn('打开 AstrBot 后台','astrbot:open')}</div>`));
}
function hideSetupTip(){if(typeof clearTimeout==='function')clearTimeout(setupTipTimer);document.getElementById('setup-tooltip')?.remove();setupTipTarget=null;setupTipPinned=false}
function showSetupTip(target){
 const c=setupCatalog().find(c=>c.id===target.dataset.setupInfo);if(!c||!document.createElement)return;
 hideSetupTip();setupTipTarget=target;const tip=document.createElement('div');tip.id='setup-tooltip';tip.role='tooltip';tip.className='setup-tooltip';
 const source=c.source?.kind==='bundled'?'随安装包分发，不下载、不换镜像':c.source?.kind==='none'?'本机配置':c.source?.kind==='download'?(()=>{try{return new URL(c.source.url).hostname}catch{return '组件发布源'}})():'组件发布源';
 tip.innerHTML=`<b>${esc(c.name)} · ${esc(c.id)}</b><div>${esc(c.desc)}</div><div>影响页面：${esc(c.affects)}</div><div>${c.tier==='manual'?'手动完成':setupIsBundled(c)?'随包自带，不占下载流量':c.size_bytes>0?setupBytes(c.size_bytes):'大小未定，按阶段显示'} · ${c.tier==='required'?'运行必需':'可选'} · ${esc(source)}</div>`;
 document.body.append(tip);const r=target.getBoundingClientRect(),q=tip.getBoundingClientRect();tip.style.left=Math.max(8,Math.min(r.left,window.innerWidth-q.width-8))+'px';tip.style.top=Math.max(8,Math.min(r.bottom+8,window.innerHeight-q.height-8))+'px';
}
document.addEventListener('pointerover',e=>{if(e.pointerType==='touch')return;const b=e.target.closest?.('[data-setup-info]');if(b&&b!==setupTipTarget)setupTipTimer=setTimeout(()=>showSetupTip(b),300)});
document.addEventListener('pointerout',e=>{if(e.pointerType==='touch')return;if(e.target.closest?.('[data-setup-info]'))hideSetupTip()});
document.addEventListener('focusin',e=>{const b=e.target.closest?.('[data-setup-info]');if(b)showSetupTip(b)});
document.addEventListener('focusout',e=>{if(e.target.closest?.('[data-setup-info]'))hideSetupTip()});
function setupDiagnostics(){const missing=setupCatalog().filter(c=>c.tier!=='manual'&&(['failed','elevation'].includes(setupItem(c.id).state)||(setupItem(c.id).state==='pending'&&(c.tier==='required'||c.tier==='default'&&c.selected))));return missing.map(c=>{const it=setupItem(c.id),bundled=setupIsBundled(c),label=it.label||setupDetailText(c,it.state);return `<div class="alert-row"><div class="row-t"><b>${esc(c.name)} ${setupInfo(c)}</b><small>${esc(label)}</small></div><div class="row-c"><button data-setup-open="${esc(c.id)}">${bundled?(it.state==='pending'?'初始化':'修复'):(it.state==='pending'?'下载':'重试')}</button></div></div>`}).join('')}

function setupFeatureOffer(value){
 const id=value==='memory/1'?'ollama':value==='voice/4'?'breeze':value==='voice/1'?'edge-tts':value==='plugin/astrbot_plugin_dafeiyu_petlink/1'?'dsh-pet':'';
 if(!id||!setupCatalog().some(c=>c.id===id)||setupItem(id).state==='done'||!document.createElement)return false;
 const c=setupCatalog().find(c=>c.id===id),d=document.createElement('dialog');d.id='setup-manual-sheet';
 d.innerHTML=`<h2>需要先安装 ${esc(c.name)}</h2><p>${esc(c.desc)}</p><p>${id==='ollama'?'可选 · 1.6 GB':''}</p><div class="actions"><button data-install>去组件设置安装</button><button data-later>稍后</button></div>`;
 document.body.append(d);d.querySelector('[data-install]').onclick=()=>{d.close();d.remove();settingsSec=4;send('tab',{index:6});send('setup:start',{id});};d.querySelector('[data-later]').onclick=()=>{d.close();d.remove()};d.showModal();return true;
}
