let tideSec=0,tideUin='',tideTakeoverAsk=false;
function tidePage(){
  const t=(state&&state.tide)||{};
  const st=t.state||{};
  const accts=st.accounts||[];
  const tabs=['账号与登录','会话','消息','桥接'];
  const seg=`<div class="seg">${tabs.map((n,i)=>`<button data-tide="${i}" class="${tideSec===i?'selected':''}">${n}</button>`).join('')}</div>`;
  const starting=!!t.starting;
  const actions=starting?'<button disabled>正在启动潮信…</button>'
    :(t.reachable?btn('刷新状态','tide:refresh',false,t.busy)+btn('重启潮信','tide:restart',false,t.busy)
    :btn('启动潮信','tide:start',true,t.busy));
  let html=head('03','连线','<span class="act">（把耳机戴正）</span>QQ 走她这条线。',actions);
  const feedback=(t.error?`<p class="dim note-bad" role="alert">${esc(t.error)}</p>`:'')+(t.opMsg?`<p class="dim ${t.opOk===false?'note-bad':''}" role="status">${esc(t.opMsg)}</p>`:'');
  if(feedback)html+='<p class="dim" role="status">有操作反馈，可在二级菜单中查看。</p>';
  html+=seg;
  if(t.managed)html+=`<p class="dim tide-health">由${esc(BRAND.name)}启动 · 控制通道已握上 · 账号 ${accts.length} 个</p>`;
  if(!t.reachable){
    const setup=t.setup||(t.installed===false?'missing':'ready');
    const offNote=starting
      ?'<p class="dim" role="status">正在把潮信拉起来，第一次启动要等几秒，起来后这里会自动刷新。</p>'
      :setup==='missing'
        ?'<p class="dim note-bad">随包的潮信文件不完整或缺失。点「修复」会从安装包恢复；仍然不行就重装时序之子。</p>'
         +'<div class="actions"><button data-setup="retry" data-setup-id="hermes">修复潮信</button></div>'
        :(setup==='initializing'
          ?'<p class="dim" role="status">潮信已随包，正在初始化本地配置。第一次启动会建好账号与桥接设置，不用下载，也不用另装。</p>'
          :'<p class="dim">QQ 现在收不到消息。潮信随工作台一起装好，点下面就能起，不用下载。</p>')
         +'<div class="row"><div class="grow"><b>1 · 启动</b><small>直接用随包的 sidecar\\hermes，Node 也走随包版本。</small></div></div>'
         +'<div class="row"><div class="grow"><b>2 · 扫码</b><small>在 QQ 里登录后，账号会出现在「账号与登录」。</small></div></div>'
         +`<div class="actions">${btn('启动潮信','tide:start',true,t.busy)}</div>`;
    return html+screenMenu('tide-offline',[['启动与登录',card('潮信还没上线',offNote)],...(feedback?[['操作反馈',card('操作反馈',feedback)]]:[])]);
  }
  if(!t.managed)return html+screenMenu('tide-unmanaged',[['控制通道',empty('控制通道没握上','潮信是别的程序启动的，这里读不到它的状态。重启一次就好。','（对不上暗号）',btn('重启潮信','tide:restart',true,t.busy))],...(feedback?[['操作反馈',card('操作反馈',feedback)]]:[])]);
  const intro=html;html='';
  if(tideSec===0){
    const nat=st.native||{},runs=(nat.runs||[]).filter(r=>r.alive);
    const list=accts.length?accts.map(a=>`<div class="row"><div class="grow"><b>${esc(a.uin||'未知')}</b><small>${esc(a.adapter||'接入端')} · ${a.online?'在线':'离线'}${a.connectedAt?' · 连上于 '+esc(a.connectedAt):''}</small></div>${tag(a.online?'在线':'离线',a.online?'good':'')}${a.uin?`<button class="slim" data-tide-disconnect="${esc(a.uin)}">断开</button>`:''}</div>`).join(''):'';
    let hint='';
    if(!accts.length){
      if(runs.length)hint='<p class="dim">潮信已经用原生入口打开了 QQ（PID '+runs.map(r=>esc(String(r.pid))).join('、')+'）。扫码登录后账号会出现在这里；已经登录却没反应的话，等几秒再点右上「刷新状态」。</p>';
      else if(nat.qqRunning)hint='<p class="dim">QQ 正开着，但潮信没能接管它——Hook 只在 QQ 启动那一刻接上去。要让潮信接管：点下面的「退出 QQ 并接管」，它会先请 QQ 正常退出，再用原生入口重新打开；也可以自己完全退出 QQ 后点「用潮信启动 QQ」。</p><div class="actions">'+btn('退出 QQ 并接管','tide_takeover_ask',true,t.busy)+'</div>';
      else if(nat.available)hint='<p class="dim">还没有账号登录。QQ 现在没开着——点下面的「用潮信启动 QQ」，在弹出的 QQ 里扫码，账号就会出现在这里。</p><div class="actions">'+btn('用潮信启动 QQ','tide:native_start',true,t.busy)+'</div>';
      else hint='<p class="dim">原生注入组件没就绪（需要 Windows x64 和随包的原生组件）。请先在「体检」检查组件安装；外部 OneBot 端需要专用适配器，不能直接连接原生 /bridge。</p>';
    }
    html+=card('她登录的账号',list+hint);
    html+=tideNetworkStatus(st);
    if(tideTakeoverAsk)html+=card('接管前确认','<p class="dim note-bad">这一步会先请正在运行的 QQ 正常退出（和你点 QQ 自己的退出一样），再从潮信的原生入口重新打开。正在聊的会话会短暂断开，聊天记录不删。</p><div class="actions"><button class="primary" data-action="tide:native_takeover">知道了，退出 QQ 并接管</button><button data-takeover-cancel="1">算了</button></div>');
  } else if(tideSec===1){
    html+=card('会话',accts.length?accts.map(a=>{const ms=(t.messages&&t.messages[a.uin])||[];return `<div class="row"><div class="grow"><b>${esc(a.uin||'')}</b><small>${a.online?'在线':'离线'} · ${esc(a.adapter||'')}${a.lastSeen?' · 最后 '+esc(a.lastSeen):''}</small><small>${(a.capabilities||[]).join(' · ')||''}</small></div><span class="chip">${ms.length} 条</span><button class="slim" data-tide-view="${esc(a.uin||'')}">看消息</button></div>`}).join(''):'<p class="dim">还没有会话。</p>');
  } else if(tideSec===2){
    if(!tideUin&&accts.length)tideUin=accts[0].uin||'';
    const ms=(t.messages&&t.messages[tideUin])||[];
    html+=card('最近的消息',`<div class="actions">${accts.map(a=>`<button class="slim${a.uin===tideUin?' primary':''}" data-tide-view="${esc(a.uin||'')}">${esc(a.uin||'')}</button>`).join('')}</div>`+(ms.length?`<div class="logbox">${ms.slice().reverse().map(m=>{const p=m.payload||{};const who=(p.sender&&(p.sender.nickname||p.sender.user_id))||'？';const text=typeof p.message==='string'?p.message:(p.raw_message||'[非文本消息]');const when=p.time?new Date(p.time*1000).toLocaleString('zh-CN',{hour12:false}):'';return `<div class="msg"><b>${esc(String(who))}</b><small>${esc(when)}${m.saved?' · 已存':''}</small><p>${esc(String(text))}</p>`}).join('')}</div>`:'<p class="dim">这个账号还没有消息。</p>'));
    html+=card('发一条',`<form id="tide-send-form" data-uin="${esc(tideUin)}" class="form"><label>发给<select name="kind"><option value="private">私聊</option><option value="group">群里</option></select></label><label>QQ 号 / 群号<input name="target" inputmode="numeric" required></label><label>内容<textarea name="text" rows="3" required></textarea></label><div class="actions"><button class="primary" type="submit">发出去</button></div></form>`);
  } else if(tideSec===3){
    const b=st.bridge||{},nat=st.native||{};
    html+=card('接入端',`<div class="row"><div class="grow"><b>${esc(st.name||'潮信')}</b><small>${esc(st.version||'')}${st.uptime?' · 已运行 '+esc(String(st.uptime))+' 秒':''}</small></div>${tag(t.managed?'托管中':'独立运行',t.managed?'good':'')}</div>`);
    const runs=(nat.runs||[]);
    const runRows=runs.length?runs.map((r,i)=>`<div class="row"><div class="grow"><b>原生运行 ${i+1}</b><small>PID ${esc(String(r.pid))} · ${r.alive?'进程在':'已退出'} · 阶段 ${esc(r.stage||'')}${r.detail?' · '+esc(r.detail):''}</small></div>${r.alive?`<button class="slim" data-tide-stop="${esc(r.id)}">停止注入</button>`:''}</div>`).join(''):'';
    html+=card('桥与原生',`<div class="row"><div class="grow"><b>原生适配器地址（不是 AstrBot OneBot 地址）</b><small>${esc(b.url||'未上报')}</small></div></div><div class="row"><div class="grow"><b>原生 QQ 注入</b><small>${nat.available?'可用':'不可用'}${nat.qqRunning?' · QQ 正在运行':''}${nat.running?' · 已由潮信托管':''}</small></div>${nat.qqRunning?btn('退出 QQ 并接管','tide_takeover_ask',false,t.busy):btn('用潮信启动 QQ','tide:native_start',false,t.busy)}</div>`+runRows);
    html+=tideNetworkStatus(st);
    html+=card('调试网页',`<p class="dim">独立网页只是排障用的只读面板，默认关闭。要用时先给潮信设环境变量 HERMES_WEBUI=1 再启动，然后打开 http://127.0.0.1:25109/ ；它只能看，不能改配置、不能发消息。平时在时序之子里管潮信就够了。</p>`);
  }
  return intro+screenCards('tide-'+tideSec,html+(feedback?card('操作反馈',feedback):''),tideTakeoverAsk&&tideSec===0?2:-1);
}
function tideNetworkStatus(st){
  const labels={connected:'已连接',listening:'监听中',ready:'就绪',connecting:'正在连接',reconnecting:'等待重连',disabled:'已关闭',error:'连接失败'};
  const rows=(st.networks||[]).map(n=>`<div class="row"><div class="grow"><b>${esc(n.name||'OneBot')} · ${esc(labels[n.status]||n.status||'未知')}</b><small>${esc(n.address||'')}${n.error?' · '+esc(n.error):''}</small>${['error','reconnecting'].includes(n.status)?`<small>${n.error==='UNAUTHORIZED'?'鉴权失败：核对两边 OneBot 令牌，不要填工作台托管令牌。':n.error==='EADDRINUSE'?'端口被占用：关闭重复接入端，或在配置中使用空闲端口。':'先启动 AstrBot，再检查 6199 监听与 OneBot 令牌；潮信会自动重连。'}</small>`:''}</div></div>`).join('');
  return card('与 AstrBot 的连接',rows||'<p class="dim">账号接入后才会建立 OneBot 节点。安装预设使用 ws://127.0.0.1:6199/ws；仍没有节点时请运行自设定并检查潮信预配置。</p>');
}

