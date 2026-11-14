// 体检：只列需要处理的事；全好就说一句没有。
function check(){
  const items=(state.items||[]);
  const bad=items.filter(i=>!/正常|在线|已好|已装|ok/i.test(i.state||''));
  const row=i=>`<div class="alert-row">`
    +`<div class="row-t"><b>${esc(i.name)}</b><small>${esc(i.text||'')}</small></div>`
    +`<div class="row-c"><span class="pill bad">${esc(i.state||'待处理')}</span>`
    +(i.fix?btn(i.fixLabel||'修一下','fix:'+i.fix,true,state.maintBusy):'')+`</div></div>`;
  return head('05','体检','<span class="act">（挨个看了）</span>只列要动手的，没问题的就不念了。',
      btn('重新体检','btn:diag',false,state.maintBusy))
    +screenMenu('diagnostics',[
      ['待处理',(bad.length||setupDiagnostics())?panel('要处理的事',setupDiagnostics()+bad.map(row).join('')):`<div class="empty" role="status"><b>现在没有卡住的地方。</b><small>体检过一遍了，都在。</small></div>`],
      ['完整体检',card('完整体检结果',`<pre>${esc(state.check||'尚未体检。')}</pre>`)],
      ['通知详情',state.notify?card(state.notify.title,`<p>${esc(state.notify.body)}</p><div class="actions">${btn('重启一下','notify:fix',true)}${btn('知道了','notify:close')}</div>`):empty('没有待处理通知')]]);
}
