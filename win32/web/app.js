let styleBefore='',watchDraft;
function nav(){
 const style=styleNow(state);
 $('nav').innerHTML=styleNav().map(([tab,label],i)=>{
  const n=navOf(tab),on=navActive(state.tab)===tab;
  const group=style==='barrier'&&[0,3].includes(i)?`<span class="nav-group">${i===0?'日常':'管理'}</span>`:'';
  return group+`<button data-nav="${tab}" class="${on?'active':''}"${on?' aria-current="page"':''} aria-label="${esc(label+'，'+n.hint)}" title="${esc(n.hint)}"><span class="nav-no" aria-hidden="true">${String(i+1).padStart(2,'0')}</span><span class="ic" aria-hidden="true">${n.icon}</span><span class="lb">${label}</span><kbd aria-hidden="true">${i+1}</kbd></button>`;
 }).join('');
}
function paint(){
  const pages=[overview,bili,records,records,check,maintenance,settings,tidePage];
  captureFocus();
  $('page').innerHTML=setupThinProgress()+tideLine(state)+routePager()+directionPage('<div class="screen-route">'+(pages[state.tab]||overview)()+'</div>');
  mountScreens();
  restoreFocus();
  setupAnnounce();
  startTicker();
}
function render(s){
  scheduleScreenFit();
  state=s;
  applyTheme(s);
  if(s.wizardComplete===false){renderOnboarding();return;}
  onboardingSignature="";
  // 记下每个耗时操作开始的时刻，只用于显示「已等 N 秒」；后端一结束就清掉。
  for(const key of ['watchBusy','feedBusy','checkBusy','maintBusy','dshBusy','updateBusy']){
    if(s[key]){if(!busySince[key])busySince[key]=Date.now()}
    else delete busySince[key];
  }
  const set=(id,v)=>{const e=$(id);if(e)e.textContent=v};
  set('clock',clockText(s.clock));
  set('workspace',s.workspace||'');
  set('side-version',s.version||'');
  set('chip',s.version||'');
  set('crumb',navOf(s.tab).label||'');
  set('bar-hint',navOf(s.tab).hint||'');
  set('connection',bridge?'连着':'预览');
  set('feature-feedback',s.lastOp||'');
  set('feature-progress',s.featureSettings?.progress?.text||'');
  set('config-feedback',s.configNote||'填完点保存。测试使用当前填写的参数，测试成功后请保存。');
  const apiTest=$('api-test');if(apiTest){apiTest.disabled=!!s.apiTestBusy;apiTest.textContent=s.apiTestBusy?'正在测试…':'测一下连接'}
  const t=$('toast');
  if(t){t.textContent=s.toast||'';if(t.classList)t.classList.toggle('show',!!s.toast)}
  const note=$('notification');if(note)note.innerHTML=s.notify?card('有一条通知',`<p>${esc(Array.from(s.notify.title||'状态发生变化').slice(0,64).join(''))}</p><div class="actions"><button data-notice-details>查看详情</button>${btn('知道了','notify:close')}</div>`):'';
  const changed=tabBefore!==s.tab; if(changed){signature='';tabBefore=s.tab;barrierOpen=false;barrierTodayOpen=false;gateFormDirty=false;watchDraft=undefined}
  const styleChanged=styleBefore!==styleNow(s);styleBefore=styleNow(s);
  if(changed||styleChanged||!$('nav').children.length){captureFocus();nav();restoreFocus()}
  const comparable=Object.assign({},s,{clock:'',toast:'',notify:s.tab===4?s.notify:null});
  const sig=JSON.stringify([comparable,helpSec,cfgSec,maintSec,tideSec,tideUin]);
  if(sig===signature&&!force)return;
  const focused=document.activeElement,form=$('config-form');
  if(!changed&&!force&&(form||(themeDirty&&$('theme-form'))||(featureDirty&&$('feature-form'))||(gateFormDirty&&($('tide-settings-form')||$('tide-send-form')))||(focused&&focused.matches('input,textarea,select'))||$('editor').open))return;
  const draft=$('watch-input')?$('watch-input').value:watchDraft;if(!changed)watchDraft=draft;
  signature=sig;force=false;
  paint();
  if(draft!==undefined&&!changed&&$('watch-input'))$('watch-input').value=draft;
}
document.addEventListener('click',e=>{
  if(setupClick(e))return;
  const setupOpen=e.target.closest?.('[data-setup-open]');if(setupOpen&&setupOpen.dataset.setupOpen!==undefined){settingsSec=4;force=true;send('tab',{index:6});send('setup:retry',{id:setupOpen.dataset.setupOpen});return;}
  if(screenClick(e))return;
  if(e.target&&e.target.matches&&e.target.matches('input[name=theme-pick]')){saveTheme(e.target.value);return}
  if(e.target&&e.target.matches&&e.target.matches('input[name=style-pick]')){saveStyle(e.target.value);return}
  if(componentOpen>=0&&!e.target.closest?.('.component-list')){componentOpen=-1;force=true;render(state)}
  const b=e.target.closest('button,[data-job]');if(!b)return;
  const d=b.dataset;
  if('wizard' in d){wizardCommand(d.wizard);return;}
  if('updateChannel' in d){send('update-channel',{value:d.updateChannel});return;}
  if('updateRollback' in d){send('update-rollback');return;}
  if('settings' in d){settingsSec=+d.settings;force=true;render(state);return;}
  if(state?.wizardComplete===false && ('nav' in d || 'job' in d))return;
  if('noticeDetails'in d){screenChoices.set('diagnostics',2);force=true;send('tab',{index:4});return}
  if('gate'in d){toggleGate(d.gate);return}
  if('protocol'in d){send('protocol',{value:d.protocol});return}
  if('todayOpen'in d){const m=document.querySelector?.('[data-screen-menu="today"]');if(m)chooseScreen(m,1);return}
  if('component'in d){componentOpen=componentOpen===+d.component?-1:+d.component;force=true;render(state);return}
  if('featureReset'in d){featureDirty=false;force=true;render(state);return}
  if('featurePanel'in d){if(featureDirty&&featurePanel!==d.featurePanel){const f=$('feature-feedback');if(f)f.textContent='当前配置有修改，请先保存后再切换';return}featurePanel=d.featurePanel;featureConfigOpen=true;maintSec=0;force=true;render(state);return}
  if('featureBack'in d){if(featureDirty){const f=$('feature-feedback');if(f)f.textContent='当前配置有修改，请先保存或放弃修改，再返回';return}featureConfigOpen=false;force=true;render(state);return}
  if('maint'in d){maintSec=+d.maint;themeDirty=false;force=true;render(state);return}
  if('theme'in d){saveTheme(d.theme);return}
  if('nav'in d){force=true;if(d.maintGo!==undefined)maintSec=+d.maintGo;const i=+d.nav;if(d.sectionGo!==undefined){send('tab',{index:i});send('section',{index:+d.sectionGo})}else send('tab',{index:i});return}
  if('testApi'in d){const form=$('config-form');if(form)send('configtest',{value:'link',fields:configFields(form)});return}
  if('biliAgent'in d){send('biliagent',{value:d.biliAgent});return}
  if(d.action==='tide_takeover_ask'){tideSec=0;tideTakeoverAsk=true;force=true;render(state);return}
  if('takeoverCancel'in d){tideTakeoverAsk=false;force=true;render(state);return}
  if('tideStop'in d){send('action',{value:'tide:native_stop',id:d.tideStop});return}
  if('action'in d){act(d.action);return}
  if('watch'in d){const el=$('watch-input');send('watch',{value:el?el.value:''});if(el)el.blur();return}
  if('feed'in d){send('feed');return}
  if('bsec'in d){force=true;send('section',{index:+d.bsec});return}
  if('configTab'in d){cfgSec=+d.configTab;force=true;render(state);return}
  if('help'in d){helpSec=+d.help;force=true;if(helpSec===1)act('up:refresh');else render(state);return}
  if('tide'in d){tideSec=+d.tide;force=true;render(state);return}
  if('tideView'in d){tideUin=d.tideView;tideSec=2;force=true;render(state);return}
  if('tideDisconnect'in d){send('action',{value:'tide:disconnect',uin:d.tideDisconnect});return}
  if('pick'in d){force=true;send('pick',{index:+d.pick});return}
  if('job'in d){screenChoices.set('records-jobs',styleNow(state)==='array'?0:1);force=true;send('job',{index:+d.job});if(state.tab!==2)send('tab',{index:2});return}
  if('log'in d){force=true;send('log',{index:+d.log});return}
  if('check'in d){force=true;send('check',{index:+d.check});return}
  if('service'in d){send('service',{index:+d.service,value:d.value||''});return}
  if('doc'in d){screenChoices.set('help-docs',1);force=true;send('doc',{index:+d.doc});return}
  if('askcopy'in d){send('askcopy',{index:+d.askcopy});return}
  if('askedit'in d){editing=+d.askedit;askReturn=b;$('ask-text').value=state.asks[editing].body;$('editor').showModal();mountTextEditor($('ask-text'));const t=screenEditors.get($('ask-text'))?.proxy||$('ask-text');if(t&&t.focus)t.focus();return}
});
function configFields(form){return [...form.querySelectorAll('[data-row]')].filter(x=>!x.readOnly&&(x.type==='checkbox'?x.checked!==x.defaultChecked:x.value!==x.defaultValue)).map(x=>({row:+x.dataset.row,value:x.type==='checkbox'?(x.checked?'开':'关'):x.value}))}
document.addEventListener('submit',e=>{
  if(e.target.id==='tide-send-form'){
    e.preventDefault();const f=e.target,q=x=>f.querySelector('[name='+x+']').value;
    send('action',{value:'tide:send',uin:f.dataset.uin,kind:q('kind'),target:q('target'),text:q('text')});
    f.querySelector('[name=text]').value='';refreshTextEditors();gateFormDirty=false;return;
  }
  if(e.target.id==='tide-settings-form'){
    e.preventDefault();const f=e.target,q=x=>f.querySelector('[name='+x+']');
    send('action',{value:'tide:settings',theme:q('theme').value,accent:q('accent').value,notes:q('notes').value,logLevel:q('logLevel').value,logRetainDays:Number(q('logRetainDays').value),reducedMotion:q('reducedMotion').checked});
    gateFormDirty=false;return;
  }
  if(e.target.id==='feature-form'){
    e.preventDefault();const form=e.target,fields={};
    for(const x of form.querySelectorAll('[data-feature-field]')){
      if(x.type==='password'&&!x.value)continue;
      fields[x.dataset.featureField]=x.type==='number'?Number(x.value):x.dataset.featureType==='boolean'?x.value==='true':x.value;
    }
    const action=e.submitter?.dataset.featureIntent||'save';
    send('featureconfig',{kind:form.dataset.kind,action,fields});
    form.querySelectorAll('input[type=password]').forEach(x=>x.value='');featureDirty=false;return;
  }
  if(e.target.id==='theme-form'){e.preventDefault();saveTheme('custom');return}
  if(e.target.id!=='config-form')return;e.preventDefault();
  const form=e.target;
  send('configsave',{value:form.dataset.kind,fields:configFields(form)});
  form.querySelectorAll('input[type=password]').forEach(x=>x.value='');
});
document.addEventListener('mousedown',e=>{
 if(e.button===0&&e.target.closest('.bar,.me')&&!e.target.closest('button,input,select,a'))send('window',{value:e.detail===2?'maximize':'drag'});
});
document.addEventListener('input',e=>{if(e.target.closest('#theme-form'))themeDirty=true;if(e.target.closest('#feature-form'))featureDirty=true;if(e.target.closest('#tide-settings-form,#tide-send-form'))gateFormDirty=true});
document.addEventListener('change',e=>{if(setupChange(e))return;if(e.target.closest('#feature-form'))featureDirty=true;if(e.target.closest('#tide-settings-form,#tide-send-form'))gateFormDirty=true});
$('save-ask').onclick=()=>{send('asksave',{index:editing,value:$('ask-text').value});$('editor').close();force=true};
$('cancel-ask').onclick=()=>$('editor').close();
// 弹窗：Tab 只在自己里面转，Esc 走浏览器原生的取消，关掉后焦点回到原来那个按钮。
let askReturn=null;
(function(){
  const dlg=$('editor');if(!dlg||typeof dlg.addEventListener!=='function')return;
  dlg.addEventListener('keydown',e=>{
    if(e.key!=='Tab')return;
    const f=[].slice.call(dlg.querySelectorAll('button,textarea,input')).filter(x=>!x.disabled&&!x.hidden&&!x.closest('[hidden]'));
    if(!f.length)return;
    const first=f[0],last=f[f.length-1];
    if(e.shiftKey&&document.activeElement===first){e.preventDefault();last.focus()}
    else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first.focus()}
  });
  dlg.addEventListener('close',()=>{if(askReturn&&askReturn.focus)askReturn.focus();askReturn=null});
})();
document.addEventListener('keydown',e=>{
  if(e.key==='Escape')hideSetupTip();
  if(state?.wizardComplete===false&&state.wizardManual&&e.key==='Escape'){e.preventDefault?.();send('setup:settings-back');return;}
  if(state?.wizardComplete===false){if(e.key==='Escape'){e.preventDefault?.();wizardCommand('exit')}return;}
  if(e.key==='Escape'&&tideTakeoverAsk){tideTakeoverAsk=false;force=true;render(state);return}
  if(e.key==='Escape'&&componentOpen>=0){componentOpen=-1;force=true;render(state);return}
  if(e.key==='Escape'&&styleNow(state)==='barrier'&&(barrierOpen||barrierTodayOpen)&&!$('editor')?.open){e.preventDefault?.();toggleGate(barrierTodayOpen?'today':state.tab===2?'record':'page');return}
  if(e.target.matches('input,textarea,select')){
    if(e.key==='Enter'&&e.target.id==='watch-input'){send('watch',{value:e.target.value});e.target.blur()}
    return;
  }
  // 本子上的记录是 div 扮的按钮，Tab 能走到就得能用回车/空格按下去。
  const row=e.target.closest?e.target.closest('[data-job]'):null;
  if(row&&(e.key==='Enter'||e.key===' ')){e.preventDefault();force=true;send('job',{index:+row.dataset.job});if(!state||state.tab!==2)send('tab',{index:2});return}
  if(e.altKey&&styleNow(state)==='tower'&&['ArrowUp','ArrowDown'].includes(e.key)){e.preventDefault();const a=styleNav(),i=a.findIndex(n=>n[0]===navActive(state.tab));send('tab',{index:a[Math.max(0,Math.min(a.length-1,i+(e.key==='ArrowDown'?1:-1)))][0]});return;}
  if(e.ctrlKey||e.altKey||e.metaKey||$('editor').open)return;
  // 长日志/长文档分页后不用去够鼠标：PageUp/PageDown 翻页，Home/End 到首尾。
  if(['PageUp','PageDown','Home','End'].includes(e.key)&&typeof screenPageKey==='function'&&screenPageKey(e.key)){e.preventDefault?.();return}
  const nk=/^([1-6])$/.exec(e.key);if(nk){e.preventDefault();send('tab',{index:styleNav()[+nk[1]-1][0]})}
  if(e.key==='0')send('tab',{index:6});
  if(e.key==='F9'){e.preventDefault();act('btn:allstart')}
  if(e.key==='F10'){e.preventDefault();act('btn:allstop')}
  if(e.key==='F5'){e.preventDefault();if(state&&state.tab===2)act('btn:jobs_refresh');else if(state&&state.tab===3)act('btn:log_refresh');else if(state&&state.tab===1)send('feed');else act('mref')}
});
if(bridge){bridge.addEventListener('message',e=>render(e.data));send('ready')}
else{$('page').innerHTML=head('—','时序之子','界面预览。')+`<section class="card">${empty('得从工作台打开','页面里的状态和数据都是本机接口给的，这里只是长相')}</section>`;nav_hint()}
function nav_hint(){if(!$('nav'))return;$('nav').innerHTML=NAV.map((n,i)=>`<button data-nav="${n.tab}" title="${esc(n.hint)}"><span class="ic">${n.icon}</span><span class="lb">${n.label}</span><kbd>${i+1}</kbd></button>`).join('');}
</script>
</body></html>
