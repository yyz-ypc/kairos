function artFor(kind){return STYLE_ART[styleNow(state)]?.[kind]||''}
const canonicalTheme=v=>({dark:'nightfall',light:'daylight',contrast:'boundary'}[v]||v);
const $=id=>document.getElementById(id);
const esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const bridge=window.chrome?.webview;
const ICON={
  grid:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="3.5" y="3.5" width="7" height="7" rx="2"/><rect x="13.5" y="3.5" width="7" height="7" rx="2"/><rect x="3.5" y="13.5" width="7" height="7" rx="2"/><rect x="13.5" y="13.5" width="7" height="7" rx="2"/></svg>',
  play:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="3.5" y="4.5" width="17" height="15" rx="3"/><path d="M10.4 9.3l4.4 2.7-4.4 2.7z" fill="currentColor" stroke="none"/></svg>',
  book:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M5 5.5h9a3 3 0 0 1 3 3V19H8a3 3 0 0 1-3-3z"/><path d="M5 16.5h12"/><path d="M8.5 8.5h5"/></svg>',
  term:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="3.5" y="4.5" width="17" height="15" rx="3"/><path d="M7.5 9.5l2.9 2.4-2.9 2.4M12.5 14.8h4"/></svg>',
  pulse:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M3.5 12.5h3.7l2-5 3 10 2.1-5h6.2"/></svg>',
  care:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M12 20s-7.2-4.3-7.2-9.2A4.1 4.1 0 0 1 12 8.1a4.1 4.1 0 0 1 7.2 2.7C19.2 15.7 12 20 12 20z"/></svg>',
  help:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8.4"/><path d="M9.7 9.5a2.4 2.4 0 1 1 3.2 2.2c-.8.3-1 .9-1 1.7"/><circle cx="11.9" cy="16.4" r=".85" fill="currentColor" stroke="none"/></svg>',
  cloud:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M7 18h10a3.5 3.5 0 0 0 .4-7A5 5 0 0 0 7.6 9.6 4.2 4.2 0 0 0 7 18z"/></svg>',
  chat:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M4 6.5a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v7a2 2 0 0 1-2 2H9l-4.2 3.2a.4.4 0 0 1-.8-.3z"/></svg>'
};
// 导航是六个页面；tab 是后端的页号，1（一起看看）和 2（做过的事）是子页面，不占导航位。
const NAV=[
  {tab:0,label:'今日',hint:'她今天怎么样',icon:ICON.grid},
  {tab:5,label:'她',hint:'开关、维护和设置',icon:ICON.care},
  {tab:7,label:'连线',hint:'QQ 走她这条线',icon:ICON.chat},
  {tab:3,label:'记录',hint:'做过的事和运行日志',icon:ICON.book},
  {tab:4,label:'体检',hint:'挨个查一遍',icon:ICON.pulse},
  {tab:6,label:'设置',hint:'外观、模型、AstrBot 与帮助',icon:ICON.help}
];
const navOf=tab=>NAV.find(n=>n.tab===tab)||NAV[0];
const navActive=tab=>tab===1?0:tab===2?3:tab;
let state=null,sec=0,helpSec=0,cfgSec=0,maintSec=0,editing=-1,signature='',tabBefore=-1,force=false;
const tplus=(sec2)=>new Date(Date.now()+sec2*1000);

function send(cmd,extra={}){if(bridge)bridge.postMessage({cmd,...extra});else toast('现在是界面预览，从工作台打开才能真的动手')}
function act(value){if(typeof setupFeatureOffer==='function'&&setupFeatureOffer(value))return;send('action',{value})}
function toast(t){const el=$('toast');if(!el)return;el.textContent=t;el.classList.add('show');clearTimeout(toast._t);toast._t=setTimeout(()=>el.classList.remove('show'),4000)}

function clockInfo(s){
  const m=/(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2})/.exec(s||'');
  if(!m)return{hour:new Date().getHours(),date:'',time:''};
  return{hour:+m[4],date:m[1]+'-'+m[2]+'-'+m[3],time:m[4]+':'+m[5]};
}
function clockText(s){
  const m=/(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2})/.exec(s||'');
  return m?`${m[2]}-${m[3]} ${m[4]}:${m[5]}`:(s||'');
}
function hello(hour){
  if(hour<5)return['还没睡啊','（打了个哈欠）陪你一会儿，然后都去躺着'];
  if(hour<11)return['早…','（往被子里缩了缩）再五分钟'];
  if(hour<18)return['在','（托腮）下午了，想干点什么'];
  if(hour<23)return['晚上好','（伸了个懒腰）今天想干点什么'];
  return['这么晚了','（合上电脑）有事说事，说完去睡'];
}
function btn(text,action,primary,disabled){return `<button data-action="${esc(action)}"${primary?' class="primary"':''}${disabled?' disabled':''}>${esc(text)}</button>`}
function head(no,title,line,actions,note){return `<div class="head"><div><span class="no">${esc(no)}</span><h1>${esc(title)}</h1><p>${line}${note?`<span class="meta"> · ${esc(note)}</span>`:''}</p></div><div class="head-a">${actions||''}</div></div>`}
function card(title,body,aside){return `<section class="card"><div class="card-h"><h2>${esc(title)}</h2>${aside||''}</div>${body}</section>`}
function empty(title,sub,face,action){return `<div class="empty"><img class="empty-art" src="${artFor('empty')}" alt=""><div class="face">${esc(face||'（摊手）')}</div><b>${esc(title)}</b>${sub?`<small>${esc(sub)}</small>`:''}${action?`<div class="actions" style="justify-content:center;margin-top:10px">${action}</div>`:''}</div>`}
function tag(word,cls){return `<span class="pill ${cls||''}">${esc(word)}</span>`}
// 只记录前端第一次看到 busy 的时刻；秒数只用来区分「在跑」和「卡住」，不当成后端进度。
const busySince={};let tickerOn=false;
function busyMark(key,label){
  if(!state||!state[key])return '';
  // 秒数每秒在变，不能进无障碍树，否则读屏会把「已等 N 秒」一遍遍念出来。
  return `<span class="busy"><i aria-hidden="true"></i>${esc(label)}<span data-elapsed="${key}" aria-hidden="true"></span></span>`;
}
function startTicker(){
  if(tickerOn||typeof setInterval!=='function')return;
  if(!document.querySelectorAll)return;
  tickerOn=true;
  setInterval(()=>{
    const now=Date.now();
    for(const el of document.querySelectorAll('[data-elapsed]')){
      const t=busySince[el.getAttribute('data-elapsed')];
      el.textContent=t?'　已等 '+Math.max(0,Math.round((now-t)/1000))+' 秒':'';
    }
  },1000);
}
// 整块重绘会把按钮换成新节点，键盘焦点就掉回 body 了；按数据属性认回原来那个按钮。
let focusMem='';
function focusKeyOf(el){
  if(el.tagName==='INPUT'&&el.type==='radio')return 'INPUT|radio='+el.name+':'+el.value;
  const d=el.dataset||{};
  const keys=Object.keys(d).sort();
  if(keys.length)return el.tagName+'|'+keys.map(k=>k+'='+d[k]).join('&');
  return el.tagName+'|'+(el.textContent||'').replace(/\s+/g,' ').trim().slice(0,24);
}
function captureFocus(){
  if(!document.querySelectorAll)return;
  focusMem='';
  const a=document.activeElement;
  if(!a||a===document.body||typeof a.matches!=='function')return;
  if(!a.matches('button,[data-job],[role="button"],input[type="radio"]'))return;
  if($('editor')&&$('editor').open)return;
  focusMem=focusKeyOf(a);
}
function restoreFocus(){
  if(!focusMem||!document.querySelectorAll)return;
  const want=focusMem;focusMem='';
  for(const el of document.querySelectorAll('button,[data-job],[role="button"],input[type="radio"]')){
    if(focusKeyOf(el)!==want)continue;
    if(el.focus){try{el.focus({preventScroll:true})}catch(_){el.focus()}}
    return;
  }
}
function time(t){if(!t)return '—';const d=new Date(t*1000);return d.toLocaleString('zh-CN',{month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',hour12:false})}
function inputBox(){return `<div class="input-row"><input id="watch-input" aria-label="视频或网页链接" placeholder="BV 号、链接，或者一篇看得头疼的文章" value="${esc(state.input)}"><button class="primary" data-watch${state.watchBusy?' disabled':''}>${state.watchBusy?'正在看…':'让她看看'}</button></div>${busyMark('watchBusy','看着呢')}`}
function statRow(){const unit=['条','条','','条'];return `<div class="stats">${['今天的消息','她的回复','用掉的 token','记住的事'].map((n,i)=>`<div class="stat"><b>${n}</b><strong>${esc(state.stats[i])}<small>${unit[i]}</small></strong></div>`).join('')}</div>`}
function svcRows(){return state.services.map((s,i)=>`<div class="svc"><span class="svc-i">${i?ICON.chat:ICON.cloud}</span><div class="svc-t"><strong>${esc(s.name)}</strong><small>${esc(s.desc)}${s.port?' · :'+esc(s.port):''}${s.up&&s.pid?' · PID '+esc(s.pid):''}</small></div>${s.up?tag('在跑'):tag('没起来','off')}<button class="slim" data-service="${i}" data-value="${s.up?'stop':'start'}">${s.up?'歇会':'叫起来'}</button></div>`).join('')}
function jobRows(n){return state.jobs.slice(0,n??state.jobs.length).map((j,i)=>`<div class="row link" data-job="${i}" role="button" tabindex="0" aria-label="第 ${i+1} 条：${esc(j.title)||'没名字的任务'}，${esc(j.status)}"><span class="no">${String(i+1).padStart(2,'0')}</span><div class="grow"><b>${esc(j.title)||'没名字的任务'}</b><small>${esc(j.source||'')}</small></div><time>${time(j.time)}</time><span class="pill ${/ok|success|完成|done/.test(j.status)?'':'gray'}">${esc(j.status)}</span></div>`).join('')}
// 一张卡只说一个状态。kind: ok / todo / bad / mute。
function statusCard(title,kind,note,action){
  const word={ok:'正常',todo:'待办',bad:'出错',mute:'未配置'}[kind]||kind;
  return `<section class="card status ${kind}"><div class="card-h"><h2>${esc(title)}</h2><span class="pill ${kind}">${esc(word)}</span></div><p class="status-n">${esc(note||'')}</p>${action?`<div class="actions">${action}</div>`:''}</section>`;
}
function panelRow(title,note,ctrl){
  return `<div class="row"><div class="row-t"><b>${esc(title)}</b>${note?`<small>${esc(note)}</small>`:''}</div><div class="row-c">${ctrl||''}</div></div>`;
}
function panel(title,rows,foot){return `<section class="panel"><div class="panel-h"><h2>${esc(title)}</h2></div>${rows}${foot?`<div class="panel-f">${foot}</div>`:''}</section>`}
function sw(on,action,label){
  return `<button class="sw" role="switch" aria-checked="${on?'true':'false'}" aria-label="${esc(label||'开关')}" data-action="${esc(action)}"><i aria-hidden="true"></i></button>`;
}
function go(text,tab,section){return `<button data-nav="${tab}"${section!==undefined?` data-section-go="${section}"`:''}>${esc(text)}</button>`}
