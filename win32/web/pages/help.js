function mdInline(s){return esc(s).replace(/\[([^\]]+)\]\([^)]*\)/g,'$1').replace(/\*\*(.+?)\*\*/g,'<strong>$1</strong>').replace(/`([^`]+)`/g,'<code>$1</code>')}
function helpChapters(src){
  const chapters=[];let title='正文',lines=[],code=false;
  for(const line of String(src||'').split('\n')){
    if(line.startsWith('```'))code=!code;
    const heading=!code&&line.match(/^#{1,3} (.+)/);
    if(heading){if(lines.some(l=>l.trim()))chapters.push([title,lines.join('\n')]);title=heading[1].trim();lines=[]}
    lines.push(line);
  }
  if(lines.length)chapters.push([title,lines.join('\n')]);return chapters.length?chapters:[['正文','']];
}
function markdown(src){let code=false,table=false,quote=false,out='';for(let line of String(src||'').split(/\r?\n/)){if(line.startsWith('```')){out+=code?'</pre>':'<pre>';code=!code;continue}if(code){out+=esc(line)+'\n';continue}
if(/^\s*\|/.test(line)){if(quote){out+='</blockquote>';quote=false}if(!table){out+='<table>';table=true}if(!/^\|?[\s:|\-]+$/.test(line))out+='<tr>'+line.trim().replace(/^\||\|$/g,'').split('|').map(s=>'<td>'+mdInline(s.trim())+'</td>').join('')+'</tr>';continue}
if(table){out+='</table>';table=false}
const q=line.match(/^\s*>\s?(.*)$/);if(q){if(!quote){out+='<blockquote>';quote=true}out+='<p>'+mdInline(q[1])+'</p>';continue}
if(quote){out+='</blockquote>';quote=false}
if(/^\s*-{3,}\s*$/.test(line)){out+='<hr>';continue}
if(/^#{1,6} /.test(line)){out+='<h2>'+esc(line.replace(/^#+ /,''))+'</h2>';continue}
const li=line.match(/^\s*(?:[-*]|\d+[.、])\s+(.*)$/);out+=li?'<p class="li">'+mdInline(li[1])+'</p>':'<p>'+mdInline(line)+'</p>'}
return out+(code?'</pre>':'')+(table?'</table>':'')+(quote?'</blockquote>':'')}
function help(){
  const tabs=['教程','检查更新','找人帮忙'];
  let html=`<div class="seg">${tabs.map((n,i)=>`<button data-help="${i}" class="${helpSec===i?'selected':''}">${n}</button>`).join('')}</div>`;
  if(helpSec===0){
    const index=`<div class="help-index" aria-label="教程索引">${state.docs.map((d,i)=>`<button data-doc="${i}" class="help-entry"${state.docsel===i?' aria-current="true"':''}><b>${esc(d.title)}</b><small>${esc(d.hint)}</small></button>`).join('')}</div>`;
    const selected=state.docsel>=0&&state.docs[state.docsel];
    const article=selected?card(state.docs[state.docsel].title,`<article class="doc" tabindex="0" aria-label="教程正文">${markdown(state.doc)}</article>`,btn('用系统打开','docopen')):card('从这里开始','<p class="dim">选一篇教程，正文会在这里展开。版本与常用入口一直可见。</p>');
    const places=card('常用位置',`<div class="quick" style="margin-top:0">${btn('工作区','doc:base')}${btn('组件目录','doc:bots')}${btn('日志','doc:logs')}${btn('台账文件','doc:jobs')}${btn('换工作区','ws:pick')}</div><p class="dim">当前版本 ${esc(state.version||'未上报')}</p>`);
    const chapters=selected?helpChapters(state.doc).map(([title,src])=>[title,card(selected.title+' · '+title,`<article class="doc" aria-label="教程正文">${markdown(src)}</article>`,btn('用系统打开','docopen'))]):[['正文',article]];
    html+=screenMenu('help-docs',[['教程目录',index],...chapters,['常用位置',places]]);
  }
  if(helpSec===1){
    // 下载百分比、正在问版本、报错是三种不同的状态，别挤在同一句 dim 里糊过去。
    const prog=+state.updateProgress;
    html+=`<div class="actions update-channel" role="group" aria-label="更新通道">${['stable','beta'].map(c=>`<button data-update-channel="${c}" aria-pressed="${(state.updateChannel||'stable')===c}">${c==='stable'?'正式通道':'测试通道'}</button>`).join('')}<button data-update-rollback ${state.rollbackAvailable?'':'disabled'}>回滚上一版</button></div>`;
    const note=prog>=0
      ?`<div class="progress" role="progressbar" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${Math.min(100,Math.max(0,prog))}"><i style="width:${Math.min(100,Math.max(0,prog))}%"></i></div><p class="dim" role="status">${esc(state.updateMessage)||'正在下载'}<span class="progress-pct">${prog}%</span></p>`
      :state.updateBusy
        ?`<p class="dim" role="status">${busyMark('updateBusy','正在问版本')}</p>`
        :state.updateError
          ?`<p class="dim note-bad" role="alert">${esc(state.updateError)}</p>`
          :'<p class="dim">只看当前版本对应的发布通道。</p>';
    html+=screenMenu('help-updates',[['版本与更新',card('组件与工作台',note+`${(state.updates||[]).map(u=>`<div class="row"><div class="grow"><b>${esc(u.name)}</b><small>本机 ${esc(u.local)||'不知道'} → 最新 ${esc(u.latest)||(u.id==='hermes'?'随工作台':'查不到')}</small><small>${esc(u.what)}</small></div><span class="chip">${esc(({new:'有更新',latest:'最新',local:u.id==='hermes'?'随工作台':'本地版本',unknown:'查不到',missing:'没装'})[u.state]||u.state)}</span>${u.state==='new'?btn(u.id==='wb'?'一键更新':u.id==='skill'?'更新技能':'复制说明',u.id==='wb'?'up:wbgo':'upact:'+u.id):''}</div>`).join('')}`,btn('检查更新','up:refresh',false,state.updateBusy))]]);
  }
  if(helpSec===2)html+=screenBoard('help-asks',state.asks.length?state.asks.map((a,i)=>card(a.title,`<p class="ask-preview">${esc(Array.from(a.body||'').slice(0,56).join(''))}${Array.from(a.body||'').length>56?'…':''}</p><div class="actions"><button class="slim" data-askcopy="${i}">复制给 Agent</button><button class="slim" data-askedit="${i}">查看和编辑</button></div>`)).join(''):empty('还没有求助模板'),'asks-board');
  return html;
}
