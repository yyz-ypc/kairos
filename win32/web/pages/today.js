// 今日概览直接并排展示，只有实测放不下时才分页。
function hero(){
  const c=clockInfo(state.clock),g=hello(c.hour);
  return `<section class="hero"><img class="art" src="${artFor('hero')}" alt=""><img class="companion-art" src="${CHARACTER_ART.hero}" alt="冥零"><div class="hero-in">`
    +`<div class="hero-eyebrow"><b>${esc(c.time||'')}</b><span>${esc(direction().caption)}</span></div>`
    +`<h1>${esc(g[0])}</h1><p>${esc(g[1])}</p>`
    +`<div class="actions">${btn('都叫起来','btn:allstart',true)}${btn('让她们歇着','btn:allstop')}`
    +`</div></div></section>`;
}
function overview(){
  const style=styleNow(state),recent=card('最近',recentEntries(),'<button class="quiet" data-nav="2">全部记录 →</button>');
  const feed=card('扔个链接过来',`<p class="dim">视频、文章，或者分享的一段话。</p>${inputBox()}<div class="quick"><button data-nav="1" data-section-go="0">今天看啥</button>${btn('打开产物','btn:open_shotdir')}</div>`);
  const help='<div class="today-help"><button data-nav="4">体检</button><button class="quiet" data-nav="5" data-maint-go="2">组件详情 →</button></div>';
  const context=style==='array'?dependencyBoard():card('她身边',(style==='seer'?attention():'')+svcRows());
  return `<div class="screen-route today-screen ${style}-today">${screenBoard('today',hero()+feed+`<div class="board-wide">${statRow()}</div>`+context+recent+`<div class="board-wide">${help}</div>`,'dashboard-board')}</div>`;
}
