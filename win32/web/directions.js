// 三种信息组织：关注深度 / 任务与依赖 / 可见边界。主题只改变令牌。
const DIRECTIONS={
  seer:{name:'灵视',caption:'从此刻，看向留下的记忆',near:'此刻 · 前景',middle:'今日 · 中景',far:'记忆 · 远景'},
  array:{name:'术阵',caption:'选定任务，再接通所需灵引',near:'阵眼 · 当前任务',middle:'内环 · 操作',far:'外环 · 灵引'},
  barrier:{name:'界门',caption:'日常在眼前，细节由你开门',near:'常规侧',middle:'门的另一侧',far:'神秘侧'},
  tower:{name:'时塔',caption:'一层一页，抬头看自己在第几层',near:'塔顶 · 此刻',middle:'当前层',far:'塔基 · 记录'},
  folio:{name:'卷页',caption:'一页一页读，旁边留批注',near:'页眉 · 此刻',middle:'正文',far:'页边 · 批注'}
};
function styleNow(s){return Object.hasOwn(DIRECTIONS,s?.style)?s.style:'barrier'}
const direction=()=>DIRECTIONS[styleNow(state)];
let barrierOpen=false,barrierTodayOpen=false,gateFormDirty=false;
function recentEntries(){
  const jobs=state.jobs||[],feed=state.feed||[];
  if(jobs.length)return jobs.slice(0,3).map((j,i)=>`<button class="recent-entry" data-job="${i}"><b>${esc(j.title||j.bv||'一条记录')}</b><small>${esc(j.status||'')} · ${time(j.time)}</small></button>`).join('');
  if(feed.length)return feed.slice(0,3).map(f=>`<button class="recent-entry" data-nav="1" data-section-go="0"><b>${esc(f.title||f.bv||'一条视频')}</b><small>${esc(f.up||'')} · 去一起看看</small></button>`).join('');
  return '<p class="dim">今天还没记下什么。扔一条链接过来，她看完会讲给你听。</p>';
}
function attention(){
  const bad=(state.items||[]).filter(i=>!/正常|在线|已好|已装|ok/i.test(i.state||''));
  const down=(state.services||[]).filter(s=>!s.up);
  const note=bad[0]?`${bad[0].name} · ${bad[0].text||bad[0].state}`:down.length?`${down.map(s=>s.name).join('、')} 还没启动`:'已上报的服务都在运行';
  return `<section class="attention ${bad.length||down.length?'attention-warn':''}" aria-label="此刻状态"><span class="direction-label">${esc(direction().near)}</span><p>${esc(note)}</p><button class="quiet" data-nav="4">体检 →</button></section>`;
}
function dependencyBoard(){
  const accounts=state.tide?.state?.accounts||[],online=accounts.some(a=>a.online);
  const qq=state.services?.find(s=>/潮信|SnowLuma|NapCat/.test(s.name)),brain=state.services?.find(s=>/AstrBot/.test(s.name));
  const nodes=[['QQ 会话',accounts.length?(online?'已登录':'离线'):'尚未确认'],[qq?.name||'潮信',qq?(qq.up?'进程就绪':'未启动'):'尚未确认'],['AstrBot',brain?(brain.up?'接口监听中':'未启动'):'尚未确认']];
  return `<section class="dependency-board" aria-label="外部服务依赖"><span class="direction-label">外环 · 灵引</span><ol>${nodes.map(([name,status],i)=>`<li><span class="node-index" aria-hidden="true">${i+1}</span><div><b>${esc(name)}</b><small>${esc(status)}</small></div></li>`).join('')}</ol><p class="dim">进程就绪后，还需验证实际收发。</p><button class="quiet" data-nav="7">检查连接 →</button></section>`;
}
function taskReceipt(){
  const busy=state.watchBusy||state.maintBusy||state.checkBusy||state.updateBusy;
  const text=state.watchBusy?state.watchStatus||'正在读取内容':state.maintBusy?state.note||'正在处理设置':state.checkBusy?'体检进行中':state.updateBusy?'正在检查更新':state.lastOp||'等待你选定下一件事';
  return `<div class="task-receipt" role="status"><span class="direction-label">阵眼 · ${busy?'进行中':'待命'}</span><p>${esc(text)}</p></div>`;
}
function gateHasDraft(){
  if(featureDirty||themeDirty||gateFormDirty)return true;
  const f=$('config-form');return !!f&&!!f.querySelectorAll&&configFields(f).length>0;
}
function gateControl(open,key='page'){
  return `<button class="gate-handle" data-gate="${key}" aria-expanded="${open}" aria-controls="${key==='today'?'today-side':'barrier-content'}"><span aria-hidden="true">${open?'◁':'▷'}</span>${open?'关门 · 隐藏详情':'开门 · 显示详情'}</button>`;
}
function barrierSummary(){
  return `<aside class="barrier-summary">${screenBoard('barrier-context','<section><h2>只看需要知道的</h2>'+svcRows()+'<p class="dim">账号、消息和设置详情由你打开。关门会隐藏当前页面的详情。</p><button class="quiet" data-nav="4">体检 →</button></section>','context-board')}</aside>`;
}
function directionPage(body){
  const style=styleNow(state);
  if(state.tab===0)return body;
  if(style==='seer')return `<div class="seer-workspace"><div class="seer-current"><section class="seer-focus" aria-label="聚焦当前页面">${body}</section></div><aside class="seer-memory" aria-label="记忆远景">${screenBoard('seer-context',attention()+card('刚刚发生',recentEntries()+'<button class="quiet" data-nav="2">走进记录 →</button>'),'context-board')}</aside></div>`;
  if(style==='array')return `<div class="array-workspace"><section class="array-slot" aria-label="当前术式">${body}</section><aside class="array-outside">${screenBoard('array-context',taskReceipt()+dependencyBoard()+card('执行回执',recentEntries()),'context-board')}</aside></div>`;
  if(style==='tower'){const floor=String(NAV.findIndex(n=>n.tab===navActive(state.tab))+1).padStart(2,'0');
    return `<div class="tower-workspace"><section class="tower-room" data-floor="${floor}" aria-label="当前楼层">${body}</section><aside class="tower-base loose-panel" aria-label="塔基记录">${screenBoard('tower-base',attention()+card('塔基 · 最近',recentEntries()),'base-board')}</aside></div>`}
  if(style==='folio')return `<div class="folio-workspace"><section class="folio-page" aria-label="当前页">${body}</section><aside class="folio-margin loose-panel" aria-label="页边批注">${screenBoard('folio-notes',attention()+card('页边 · 刚刚',recentEntries()),'margin-board')}</aside></div>`;
  // 深层页关门时不把账号/表单正文写进 DOM；公共页保持直接可用。
  if([5,7].includes(state.tab))return `<div class="barrier-workspace">${barrierSummary()}<div class="barrier-door">${gateControl(barrierOpen)}</div><section id="barrier-content" class="barrier-content" aria-label="神秘侧详情">${barrierOpen?body:`${head(state.tab===5?'02':'03',state.tab===5?'她':'连线','细节在门后。随时可以打开，也可以关上。')}<div class="closed-door"><span aria-hidden="true">✦　✦</span><h2>给她一点自己的空间</h2><p>开门后查看${state.tab===5?'功能、习惯与组件设置':'账号、消息与原生接管'}。</p>${gateControl(false)}</div>`}</section><p id="gate-feedback" class="gate-feedback" role="status"></p></div>`;
  return `<div class="barrier-public"><span class="side-coordinate">常规侧 · ${esc(navOf(navActive(state.tab)).label)}</span>${body}</div>`;
}
function toggleGate(key){
  const open=key==='today'?barrierTodayOpen:barrierOpen;
  if(open&&gateHasDraft()){const note=$('gate-feedback');if(note)note.textContent='当前有未保存的修改，请先保存或放弃草稿，再关门。';return}
  if(key==='today')barrierTodayOpen=!open;else barrierOpen=!open;
  force=true;render(state);
  if(open)document.querySelector?.(`[data-gate="${key}"]`)?.focus?.();
}

const STYLE_NAV={
 seer:[[0,'此刻'],[7,'连线'],[3,'记忆'],[5,'养成'],[4,'体检'],[6,'设置']],
 array:[[0,'执行'],[7,'连接'],[3,'归档'],[5,'能力'],[4,'诊断'],[6,'配置']],
 barrier:[[0,'今日'],[7,'连线'],[3,'记录'],[5,'她'],[4,'体检'],[6,'设置']],
 tower:[[0,'广场'],[5,'能力'],[7,'通讯'],[3,'档案'],[4,'检修'],[6,'控制室']],
 folio:[[0,'扉页'],[3,'档案'],[7,'来信'],[5,'能力'],[4,'校勘'],[6,'附录']]
};
function styleNav(){return STYLE_NAV[styleNow(state)]}
function routePager(){
 if(!['tower','folio'].includes(styleNow(state)))return '';
 const entries=styleNav(),i=Math.max(0,entries.findIndex(n=>n[0]===navActive(state.tab)));
 return `<nav class="route-pager" aria-label="${styleNow(state)==='tower'?'楼层切换':'章节切换'}"><button data-nav="${entries[Math.max(0,i-1)][0]}" ${i===0?'disabled':''}>← ${styleNow(state)==='tower'?'上一层':'上一章'}</button><span>${String(i+1).padStart(2,'0')} / ${entries.length} · ${esc(entries[i][1])}</span><button data-nav="${entries[Math.min(entries.length-1,i+1)][0]}" ${i===entries.length-1?'disabled':''}>${styleNow(state)==='tower'?'下一层':'下一章'} →</button></nav>`;
}
