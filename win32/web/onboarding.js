let onboardingSignature='';
const WIZARD_STEPS=['欢迎','工作区','准备组件','风格主题','连 QQ','完成'];
function wizardOnline(s){const t=s.tide||{};return !!(t.managed&&t.reachable&&(t.state?.accounts||[]).some(a=>a.online&&!a.demo&&a.uin));}
function wizardCommand(value){
 const step=Math.max(0,Math.min(5,+state.wizardStep||0));
 if((value==='next'&&step===4||value==='finish')&&!wizardOnline(state))return;
 if(step===2&&(value==='next'||value==='back')&&!setupReady())return;
 send('wizard',{value});
}
function renderOnboarding(){
 const step=Math.max(0,Math.min(5,+state.wizardStep||0)),online=wizardOnline(state);
 const feedback=$('config-feedback');if(feedback)feedback.textContent=state.configNote||'填完点保存。';
 const toast=$('toast');if(toast){toast.textContent=state.toast||'';toast.classList?.toggle('show',!!state.toast)}
 const t=state.tide||{},sig=JSON.stringify([step,state.workspace,state.style,state.theme,state.themeBase,state.theme_bg,state.theme_panel,state.theme_ink,state.theme_accent,state.setup,state.wizardManual,state.bconfig,state.configNote,t.reachable,t.managed,t.error,t.opMsg,t.starting,t.state?.accounts,t.state?.native,t.state?.networks]);
 if(sig===onboardingSignature&&$('page').innerHTML.includes('wizard-shell')&&!force)return;
 if(state.wizardManual&&$('config-form')&&!force&&$('page').dataset?.manualId===state.wizardManual)return;
 if(themeDirty&&$('theme-form'))return;
 onboardingSignature=sig;force=false;captureFocus();
 $('nav').innerHTML=WIZARD_STEPS.map((name,i)=>`<div class="wizard-step"${i===step?' aria-current="step"':''}>${i+1} · ${name}</div>`).join('');
 if($('crumb'))$('crumb').textContent='初次见面 · '+WIZARD_STEPS[step];
 let content='';
 if(step===0)content=card('欢迎来到'+BRAND.name,'<p>冥零还在这里。先选工作区、准备组件和外观，再连上 QQ，就可以开始。</p>');
 if(step===1)content=card('选择工作区',`<p class="dim">产物、组件和日志会放在这里。</p><p>${esc(state.workspace||'')}</p><div class="actions"><button data-wizard="workspace">选择文件夹</button></div>`);
 if(step===2){content=setupProgress()+setupGroups(true);if(!setupData().busy&&['idle',undefined].includes(setupData().state))send('setup:start');}
 if(step===3)content=themeSettings();
 if(step===4){tideSec=0;content=tidePage();}
 if(step===5)content=card('准备好了',`<p>${online?'QQ 已连接，冥零在等你。':'QQ 连接已断开，请返回重试。'}</p><p class="dim">模型随时可以到设置里配置。</p>`);
 if(state.wizardManual){
   $('page').innerHTML=`<div class="wizard-shell">${setupThinProgress()}<div class="wizard-content">${state.wizardManual==='qq-login'?'':head('03','准备组件 · 手动设置','完成后返回准备组件；向导步骤会保留。')}${setupManualContent(state.wizardManual)}</div><div class="wizard-controls"><button data-setup="settings-back">返回准备组件</button><button data-wizard="exit">退出，下次继续</button></div></div>`;
   if($('page').dataset)$('page').dataset.manualId=state.wizardManual;
   mountScreens();restoreFocus();return;
 }
 if($('page').dataset)delete $('page').dataset.manualId;
 const next=step===5?'finish':'next';
 const controls=`<div class="wizard-controls">${step>0&&step!==4?'<button data-wizard="back"'+(step===2&&!setupReady()?' disabled':'')+'>上一步</button>':''}${step===4?'<button data-wizard="retry">重试</button>':''}${step===2?'<button data-setup="'+(setupData().busy?'pause':'resume')+'">'+(setupData().busy?'暂停':'继续准备')+'</button>':''}<button class="primary" data-wizard="${next}"${(step>=4&&!online)||(step===2&&!setupReady())?' disabled':''}>${step===5?'进入时序之子':'下一步'}</button><button data-wizard="exit">退出，下次继续</button></div>`;
 $('page').innerHTML=`<div class="wizard-shell"><div class="wizard-content">${step===4?'':head(String(step+1).padStart(2,'0'),WIZARD_STEPS[step],'首次启动 · '+(step+1)+' / 6')}${content}</div>${controls}</div>`;
 mountScreens();restoreFocus();setupAnnounce();
}
