// 状态入口保留统一语义，各风格使用自己的空间语言。
function overall(s){
  s=s||state;
  if(!s)return 'ok';
  const t=s.tide||{};
  if(t.error)return 'error';
  if((s.items||[]).some(i=>/错|失败|error/i.test(i.state||'')))return 'error';
  if(/失败|出错/.test(s.check||''))return 'error';
  if(!t.reachable)return 'todo';
  return 'ok';
}
function tideLine(s){
  const st=overall(s);
  const word={ok:'已上报状态正常',todo:'有一件事在等你',error:'有地方出错了'}[st];
  const label={seer:'灵流',array:'术式',barrier:'边界',tower:'塔层',folio:'卷页'}[styleNow(s)];
  return `<div class="status-line" data-state="${st}" role="img" aria-label="${label}状态：${word}"><span aria-hidden="true">${word}</span></div>`;
}
