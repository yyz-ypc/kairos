// 记录：做过的事和运行日志放在同一页，顶部分段切换。
function records(){
  const done=state.tab===2;
  // 这两段是"翻到哪一页"，不是 tablist —— 它们改的是整页的 tab，按钮本身就是导航。
  const seg=`<div class="seg" role="group" aria-label="记录分类">`
    +`<button class="${done?'selected':''}"${done?' aria-current="page"':''} data-nav="2">做过的事</button>`
    +`<button class="${!done?'selected':''}"${!done?' aria-current="page"':''} data-nav="3">运行日志</button></div>`;
  return head('04','记录',done?'<span class="act">（翻本子）</span>都记着呢，一条没落。':'<span class="act">（探头）</span>出什么事了，我看看。',
      done?btn('刷新','btn:jobs_refresh'):(btn('刷新','btn:log_refresh')+btn('日志目录','btn:logdir')))
    +seg
    +(done?jobBody():logBody());
}
function jobBody(){
  const index=card('本子',state.jobs.length?`<div class="meta" style="margin-bottom:8px">最近 ${state.jobs.length} 条</div><div class="joblist">${jobRows()}</div>`:empty('本子还是空的','从一条视频或一个链接开始','（摊手）',go('去丢个链接',1,1)));
  const detail=card('这一次',state.jobDetail?`<pre tabindex="0" aria-label="记录详情">${esc(state.jobDetail)}</pre><div class="quick">${btn('复制记录','btn:copy_job')}${btn('打开产物','btn:open_shotdir')}</div>`:empty('挑一条，展开看看','选定记录后，详情会显示在这里','（翻页）'));
  const gated=styleNow(state)==='barrier'?`<div id="barrier-content">${gateControl(barrierOpen,'record')}${barrierOpen?detail:'<div class="closed-door"><h2>记录详情在门后</h2><p>打开后查看本次内容与产物。</p></div>'}</div><p id="gate-feedback" class="gate-feedback" role="status"></p>`:detail;
  return screenMenu('records-jobs',styleNow(state)==='array'?[['执行回执',gated],['回执索引',index]]:[['记录索引',index],['选定记录',gated]]);
}
function logBody(){
  const src=[BRAND.rift,'AstrBot',BRAND.hermes,BRAND.keeper];
  return `<div class="seg seg-sub">${src.map((n,i)=>`<button data-log="${i}" class="${state.logsrc===i?'selected':''}">${esc(n)}</button>`).join('')}</div>`
    +screenMenu('records-logs',[
    ['运行日志',card('运行日志',`<pre aria-label="运行日志">${esc(state.log)||'安静得很，一行都没有。'}</pre>`)],
    ['hermes 日志',card(BRAND.hermes,`<div class="logbox" aria-label="潮信日志">${(state.tide?.logs||[]).slice().reverse().map(r=>`<div class="msg"><b>${esc(r.level||'')}</b><small>${esc(String(r.time||'').replace('T',' ').slice(0,19))} · ${esc(r.scope||'')}</small><p>${esc(r.message||'')}</p></div>`).join('')||'<p class="dim">还没有潮信日志。</p>'}</div>`)]]);
}
