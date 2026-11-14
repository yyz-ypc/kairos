function bili(){
  const tabs=['今天看啥','丢个链接','看完了','配置'];
  let panes=[];
  let html=head('01·A','一起看看','<span class="act">（把椅子拖过来）</span>想看什么，丢给我。',btn('打开产物目录','btn:open_shotdir'))+`<div class="seg">${tabs.map((n,i)=>`<button data-bsec="${i}" class="${state.bsec===i?'selected':''}">${n}</button>`).join('')}</div>`;
  if(state.bsec===0){
    panes.push(['推荐内容',card('今天看啥',`<p class="dim" role="status">${esc(state.feedNote||'')}</p>${state.feedBusy?`<p class="dim" role="status">${busyMark('feedBusy','正在捞')}</p>`:''}`+(state.feed.length?state.feed.map((f,i)=>`<div class="row"><span class="no">${String(i+1).padStart(2,'0')}</span><div class="grow"><b>${esc(f.title)}</b><small>${esc(f.up)} · ${esc(f.dur)} · ${esc(f.bv)}</small></div><button class="slim" data-pick="${i}">就看这个</button></div>`).join(''):empty(state.feedBusy?'正在捞…':'今天还没挑','点一下，我随便捞几支','（挠头）')),`<button class="slim" data-feed${state.feedBusy?' disabled':''}>${state.feedBusy?'在找了…':'换一批'}</button>`)]);
    const a=state.biliAgent||{},status={pending:'待看',running:'正在看',done:'看完了',failed:'失败',cancelled:'已取消',interrupted:'上次中断',skipped:'已看过'};
    panes.push(['自动安排',card('让她自己安排',`<p class="dim">使用已保存的对话 API 选片，每轮最多三条。自动模式按 B 站配置的作息运行；工作台需保持打开。</p><p role="status">${esc(a.note||'尚未开启')}</p>${a.enabled?`<p class="dim">自动安排已开启${a.nextRefresh?' · 下次刷新不早于 '+esc(new Date(a.nextRefresh*1000).toLocaleString('zh-CN')):''}</p>`:''}<div class="actions"><button data-bili-agent="once"${a.busy?' disabled':''}>${a.busy?'正在安排…':'API 安排一轮'}</button><button data-bili-agent="enable"${a.enabled?' disabled':''}>开启自动安排</button><button data-bili-agent="stop">停止安排</button></div>${(a.tasks||[]).map(t=>`<div class="row"><div class="grow"><b>${esc(t.title)}</b><small>${esc(t.bv)} · ${esc(t.reason||'')}</small><small>${esc(new Date(t.due*1000).toLocaleString('zh-CN'))}</small></div><span class="pill">${esc(status[t.status]||t.status)}</span></div>`).join('')}`)]);
  }
  if(state.bsec===1){
    const pct=Math.max(0,Math.min(100,+state.progress||0));
    // 进度条只在真有进度或真在跑的时候出现；不跑也不推 0% 的空条。
    const bar=(state.watchBusy||pct>0)
      ? `<div class="progress" role="progressbar" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${pct}"><i style="width:${pct}%"></i></div>`
        +`<p class="dim">${pct>0?`已走 ${pct}%`:'进度还没报上来'}<span class="progress-pct">${state.watchBusy?'还在跑':'收工了'}</span></p>`
      : '';
    panes.push(['链接与进度',card('丢个链接',`<p class="dim">视频会整理成笔记，网页会留下摘要和来源。</p>${inputBox()}${bar}<p class="dim" role="status">${esc(state.watchStatus)||'还没开始。挑一个你感兴趣的。'}</p><div class="quick"><button data-bsec="2">看完了 →</button>${btn('打开日记','btn:diary')}</div>`)]);
  }
  if(state.bsec===2)panes=[['阅读结果',card(state.resultTitle||'看完了',state.resultOk?`<pre>${esc(state.result)}</pre><div class="quick">${btn('复制结果','btn:copy')}${btn('九宫格','btn:open_sheet')}${btn('封面','btn:open_cover')}${btn(state.dshBusy?'正在问…':'问一句','dsh',false,state.dshBusy)}</div>`:empty('啥也没看呢','先去丢个链接','（摊手）','<button data-nav="1" data-section-go="1">丢个链接</button>'))],['她的补充',card('她的补充',`${state.dshBusy?`<p class="dim" role="status">${busyMark('dshBusy','她在想')}</p>`:''}<pre>${esc(state.dsh||'还没问。先在阅读结果中点「问一句」。')}</pre>`)]];
  if(state.bsec===3){html+=`<div class="seg">${['B 站','链接与对话 API'].map((n,i)=>`<button data-config-tab="${i}" class="${cfgSec===i?'selected':''}">${n}</button>`).join('')}</div>`;panes=[['连接配置',configForm()]]}
  return html+screenMenu('watch-'+state.bsec+'-'+(state.bsec===3?cfgSec:0),panes);
}
function configForm(){
  const kind=cfgSec?'link':'bili',rows=(cfgSec?state.lconfig:state.bconfig)||[];
  const fields=rows.map(r=>`<label class="form-row"><span><b>${esc(r.label)}</b>${r.hint?`<small>${esc(r.hint)}</small>`:''}</span>${r.kind===2?`<input type="checkbox" data-row="${r.row??0}" ${/开|true|^1$/.test(r.value)?'checked':''}>`:`<input data-row="${r.row??0}" type="${r.kind===5?'password':'text'}" value="${esc(r.value)}" ${r.kind===4?'readonly':''} ${r.kind===5?'placeholder="已保存的密钥不显示；留空就保留" autocomplete="new-password"':''}>`}</label>`).join('');
  return card(cfgSec?'链接与对话 API':'B 站配置',`<p id="config-feedback" class="dim" role="status">${esc(state.configNote)||'填完点保存。测试使用当前填写的参数，测试成功后请保存。'}</p><form id="config-form" data-kind="${kind}">${fields}<div class="actions"><button type="submit" class="primary">保存</button>${cfgSec?`<button id="api-test" type="button" data-test-api${state.apiTestBusy?' disabled':''}>${state.apiTestBusy?'正在测试…':'测一下连接'}</button>`:''}</div></form>`);
}
