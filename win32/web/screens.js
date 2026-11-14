// 二级菜单只切换现有节点，不重建表单；草稿、密钥和焦点都留在当前会话。
const screenChoices=new Map(),screenPages=new Map();
function screenDirect(key,body){return `<section id="screen-${esc(key)}" class="screen-panel screen-direct"><div class="screen-flow">${body}</div><div class="screen-pager" aria-label="内容分页" hidden><button type="button" data-screen-step="-1">上一页</button><span role="status" aria-live="polite"></span><button type="button" data-screen-step="1">下一页</button></div></section>`}
function screenBoard(key,body,classes=''){return screenDirect(key,`<div class="screen-board ${classes}">${body}</div>`)}
function screenCards(key,body,preferred=-1){return `<section class="screen-auto" data-screen-auto="${esc(key)}" data-screen-preferred="${preferred}">${body}</section>`}
function screenMenu(key,entries){
  if(entries.length===1)return screenDirect(key,entries[0][1]);
  const selected=Math.min(screenChoices.get(key)||0,Math.max(0,entries.length-1));
  const compact=entries.length>5||entries.some(([label])=>String(label).length>14);
  return `<section class="screen-menu${compact?' menu-compact':''}" data-screen-menu="${esc(key)}"><nav class="screen-tabs" aria-label="二级菜单">${entries.map(([label],i)=>`<button type="button" data-screen-choice="${i}" aria-controls="screen-${esc(key)}-${i}"${i===selected?' aria-current="page"':''}>${esc(label)}</button>`).join('')}</nav><label class="screen-select">二级菜单<select data-screen-select>${entries.map(([label],i)=>`<option value="${i}"${i===selected?' selected':''}>${esc(label)}</option>`).join('')}</select></label>${entries.map(([label,body],i)=>`<section id="screen-${esc(key)}-${i}" class="screen-panel" aria-label="${esc(label)}"${i===selected?'':' hidden'}><div class="screen-flow">${body}</div><div class="screen-pager" aria-label="内容分页"><button type="button" data-screen-step="-1">上一页</button><span role="status" aria-live="polite"></span><button type="button" data-screen-step="1">下一页</button></div></section>`).join('')}</section>`;
}
// 纯函数：每个内容单元恰好出现一次，分页不会截断或丢弃尾部。
function screenRanges(heights,capacity,gap=8,keepWithNext=[]){
  const pages=[];let start=0,used=0;
  heights.forEach((height,i)=>{const h=Math.max(0,height),pair=keepWithNext[i]&&i+1<heights.length?h+gap+heights[i+1]:h,need=pair<=capacity?pair:h;if(i>start&&used+gap+need>capacity){pages.push([start,i]);start=i;used=0}used+=(i>start?gap:0)+h});
  if(heights.length)pages.push([start,heights.length]);
  return pages.length?pages:[[0,0]];
}
function textSlices(text,limit=180,maxLines=4){
  const chars=Array.from(String(text)),out=[];let start=0,count=0,lines=1;
  chars.forEach((ch,i)=>{if(count>=limit||(ch==='\n'&&lines>=maxLines)){out.push(chars.slice(start,i).join(''));start=i;count=0;lines=1}count++;if(ch==='\n')lines++});
  out.push(chars.slice(start).join(''));return out;
}
const screenEditors=new WeakMap();
function mountTextEditor(source){
  if(!source||!document.createElement)return;
  let edit=screenEditors.get(source);
  if(edit){if(edit.full!==source.value){edit.full=source.value;edit.parts=textSlices(edit.full,edit.limit);edit.index=0;edit.draw()}edit.resize?.();return}
  const wrap=document.createElement('div');wrap.className='screen-editor';
  const proxy=document.createElement('textarea');proxy.rows=2;proxy.setAttribute('aria-label',source.getAttribute('aria-label')||source.closest('label')?.childNodes[0]?.textContent||'编辑文字');
  const bar=document.createElement('div');bar.className='screen-editor-bar';
  const prev=document.createElement('button'),next=document.createElement('button'),label=document.createElement('span');prev.type=next.type='button';prev.textContent='上一段';next.textContent='下一段';label.setAttribute('role','status');
  const required=source.required;source.required=false;source.hidden=true;source.before(wrap);wrap.append(proxy,bar);bar.append(prev,label,next);
  proxy.dataset.screenProxy='';
  edit={proxy,full:source.value,parts:[],index:0,limit:180,draw(){proxy.value=this.parts[this.index]||'';proxy.required=required&&!this.full;prev.disabled=this.index===0;next.disabled=this.index===this.parts.length-1;label.textContent=`第 ${this.index+1} / ${this.parts.length} 段`;proxy.style.height='auto';proxy.style.height=Math.max(48,proxy.scrollHeight+2)+'px'}};
  edit.parts=textSlices(edit.full,edit.limit);screenEditors.set(source,edit);edit.draw();
  const move=step=>{edit.index=Math.max(0,Math.min(edit.parts.length-1,edit.index+step));edit.draw();proxy.focus({preventScroll:true});scheduleScreenFit()};
  prev.onclick=()=>move(-1);next.onclick=()=>move(1);
  let composing=false;
  const commit=()=>{
    const offset=edit.parts.slice(0,edit.index).join('').length+proxy.selectionStart;
    edit.parts[edit.index]=proxy.value;edit.full=edit.parts.join('');source.value=edit.full;
    if(composing){source.dispatchEvent(new Event('input',{bubbles:true}));return}
    edit.parts=textSlices(edit.full,edit.limit);let base=0;edit.index=edit.parts.length-1;
    for(let i=0;i<edit.parts.length;i++){if(offset<=base+edit.parts[i].length){edit.index=i;break}base+=edit.parts[i].length}
    edit.draw();proxy.setSelectionRange(Math.max(0,offset-base),Math.max(0,offset-base));source.dispatchEvent(new Event('input',{bubbles:true}));scheduleScreenFit();
  };
  proxy.addEventListener('compositionstart',()=>composing=true);
  proxy.addEventListener('compositionend',()=>{composing=false;commit()});
  proxy.addEventListener('input',commit);
  // 窄窗按实测宽度减少每段字符，换行不会变成文本框内部滚动。
  edit.resize=()=>{if(composing)return;const width=proxy.clientWidth||wrap.clientWidth||400;const limit=Math.max(24,Math.min(180,Math.floor((width-24)/14)*4));if(limit!==edit.limit){edit.limit=limit;edit.parts=textSlices(edit.full,limit);edit.index=Math.min(edit.index,edit.parts.length-1);edit.draw()}};
  edit.resize();
}
function refreshTextEditors(){if(!document.querySelectorAll)return;for(const source of document.querySelectorAll('textarea:not([data-screen-proxy])')){if(source.closest('.screen-editor,dialog:not([open])')||source.parentElement?.closest('[hidden]'))continue;mountTextEditor(source);screenEditors.get(source)?.resize?.()}}
let screenFrame=0,screenObserver;
function scheduleScreenFit(){if(!window.requestAnimationFrame||screenFrame)return;screenFrame=window.requestAnimationFrame(()=>{screenFrame=0;fitScreens()})}
function showScreenPage(panel,index,focus=false){
  const pages=panel._screenRanges||[[0,0]],units=panel._screenUnits||[];index=Math.max(0,Math.min(pages.length-1,index));
  panel._screenPage=index;screenPages.set(panel.id,index);
  units.forEach((node,i)=>{node.hidden=i<pages[index][0]||i>=pages[index][1]});
  const pager=panel.querySelector('.screen-pager'),buttons=pager.querySelectorAll('button');buttons[0].disabled=index===0;buttons[1].disabled=index===pages.length-1;
  pager.hidden=pages.length<=1;
  pager.querySelector('span').textContent=pages.length>1?`第 ${index+1} / ${pages.length} 页`:'';
  if(focus){const first=units.slice(...pages[index]).flatMap(n=>[n,...n.querySelectorAll('button,input,textarea,select,a')]).find(n=>!n.hidden&&n.matches('button,input,textarea,select,a'));first?.focus({preventScroll:true})}
}
function splitScreenText(node,capacity){
  // 按文本节点拆长段落，保留内联格式；原 id 留在父节点，状态推送仍能找到它。
  const text=node.textContent;if(!text||!text.trim()||text.length<=1)return false;
  const copy=node.cloneNode(true);copy.removeAttribute('id');copy.removeAttribute('tabindex');
  const parts=[];let offset=0;
  const fragment=(start,end)=>{const clone=copy.cloneNode(true),walker=document.createTreeWalker(clone,NodeFilter.SHOW_TEXT);let n,pos=0;while(n=walker.nextNode()){const length=n.textContent.length;n.textContent=n.textContent.slice(Math.max(0,start-pos),Math.max(0,Math.min(length,end-pos)));pos+=length}return clone};
  while(offset<text.length){let lo=offset+1,hi=text.length,best=lo;
    // 量高必须放在 node 内部：分段最终是 node 的子节点，宽度比 node.before() 的父容器窄
    // （pre/p 的 padding 吃掉的正是这点宽度）。放在外面量会把每段撑到几乎满页，
    // 于是 screenRanges 只能把 26px 的小标题单独占一页——长日志第一页因此空白。
    while(lo<=hi){const mid=Math.floor((lo+hi)/2),probe=fragment(offset,mid);node.append(probe);const h=probe.getBoundingClientRect().height;probe.remove();if(h<=capacity){best=mid;lo=mid+1}else hi=mid-1}
    // 不把 UTF-16 代理对拆到两页。
    if(best<text.length&&/[\uD800-\uDBFF]/.test(text[best-1]))best--;
    if(best<=offset)best=offset+2;
    parts.push(fragment(offset,best));offset=best;
  }
  if(parts.length<=1)return false;
  node.replaceChildren(...parts);node.classList.add('screen-transparent','screen-text-split');return true;
}
function fitScreens(){
  const root=$('page');if(!root?.querySelectorAll||!document.createElement)return;
  refreshTextEditors();
  for(const panel of root.querySelectorAll('.screen-panel')){
    if(panel.closest('[hidden]'))continue;
    fitScreenPanel(panel);
  }
}
// 普通面板先保留卡片网格。只有实测超高时才按整行翻页，不把短设置拆成菜单。
function boardRanges(rects,capacity,gap=10){
  const rows=[];
  rects.forEach((rect,i)=>{const last=rows[rows.length-1];if(last&&Math.abs(rect.top-last.top)<2){last.end=i+1;last.height=Math.max(last.height,rect.height)}else rows.push({start:i,end:i+1,top:rect.top,height:rect.height})});
  return screenRanges(rows.map(r=>r.height),capacity,gap).map(([a,b])=>[rows[a]?.start||0,rows[b-1]?.end||0]);
}
function fitScreenPanel(panel,reservePager=false){
    const pager=panel.querySelector('.screen-pager');
    // 侧栏/塔基这类随内容自增高的容器不分页：分页是为视口内的页面准备的，
    // 在小容器里按 4px 余量切页只会把一张卡片拆出一个空页。
    if(panel.closest&&panel.closest('.loose-panel')){
      pager.hidden=true;
      for(const n of panel.querySelectorAll('[data-screen-unit]')){n.hidden=false;delete n.dataset.screenUnit}
      for(const n of panel.querySelectorAll('.screen-transparent'))n.classList.remove('screen-transparent');
      panel._screenRanges=null;panel._screenUnits=null;return;
    }
    pager.hidden=!reservePager;
    // 分页条平时是藏着的；第一次量高时它不在，真分页时它会把 flow 压矮（约 43px）。
    // 先借它的真实高度当预留，量完立刻恢复，免得每段按「没有分页条」的高度切满。
    const wasHidden=pager.hidden;pager.hidden=false;
    const cs=typeof getComputedStyle==='function'?getComputedStyle(pager):null;
    const pagerReserve=typeof pager.getBoundingClientRect==='function'?Math.ceil(pager.getBoundingClientRect().height+(parseFloat(cs?.marginTop)||0)+(parseFloat(cs?.marginBottom)||0)):44;
    pager.hidden=wasHidden;
    const flow=panel.querySelector('.screen-flow'),capacity=Math.max(1,flow.clientHeight-4);if(!flow.clientHeight)return;
    const board=flow.querySelector?.('.screen-board');
    if(board){
      for(const n of board.querySelectorAll?.('[data-screen-unit]')||[]){n.hidden=false;delete n.dataset.screenUnit}
      for(const n of board.querySelectorAll?.('.screen-transparent:not(.screen-text-split)')||[])n.classList.remove('screen-transparent');
      board.classList?.remove('screen-transparent');
      const tiles=[...board.children];tiles.forEach(n=>n.hidden=false);
      const rects=tiles.map(n=>n.getBoundingClientRect());
      if(rects.every(r=>r.height<=capacity)){
        panel._screenUnits=tiles;panel._screenRanges=boardRanges(rects,capacity);
        if(panel._screenRanges.length>1&&!reservePager){fitScreenPanel(panel,true);return}
        showScreenPage(panel,screenPages.get(panel.id)||0);return;
      }
      board.classList.add('screen-transparent');
    }
    for(const n of flow.querySelectorAll('[data-screen-unit]')){n.hidden=false;delete n.dataset.screenUnit}
    const units=[];
    const walk=node=>{
      if(node.nodeType===3){if(node.textContent.trim()){const span=document.createElement('span');node.replaceWith(span);span.append(node);walk(span)}return}
      if(node.nodeType!==1||node.hidden||node.tagName==='STYLE')return;
      // 所有表单保留在 DOM 中，隐藏分页不影响完整表单的保存和校验。
      const structural=node.matches('section:not(.hero),aside,article,form,.card,.panel,.cols,.grid,.screen-board,.help-layout,.help-reading,.help-index,.joblist,.logbox,.component-list,.feature-fields,.feature-form-foot,.screen-transparent');
      if(structural){node.classList.add('screen-transparent');for(const child of [...node.childNodes])walk(child);return}
      let h=node.getBoundingClientRect().height;
      if(h>capacity&&!node.matches('button,input,select,textarea,.screen-editor')){
        if(!node.querySelector('button,input,select,textarea')&&node.matches('p,pre,blockquote,span,small,b,h1,h2,td,li')){
          // 给分页条留出它占的高度：否则每段都会按「没有分页条」的高度切满，
          // 标题就只能单独占一页（长日志第一页空白）。
          if(splitScreenText(node,Math.max(20,capacity-48-(reservePager?0:pagerReserve)))){for(const child of [...node.children])walk(child);return}
        }
        if(node.children.length){node.classList.add('screen-transparent');for(const child of [...node.childNodes])walk(child);return}
      }
      node.dataset.screenUnit='';units.push(node);
    };
    for(const child of [...flow.childNodes])walk(child);
    const heights=units.map(n=>n.getBoundingClientRect().height);
    panel._screenUnits=units;panel._screenRanges=screenRanges(heights,capacity,8,units.map(n=>n.matches('.group-label,.card-h,.panel-h,h2,.sec-h')));
    if(panel._screenRanges.length>1&&!reservePager){fitScreenPanel(panel,true);return}
    const active=document.activeElement,focused=units.findIndex(n=>n===active||n.contains(active));
    let page=screenPages.get(panel.id)||0;
    if(focused>=0)page=panel._screenRanges.findIndex(([a,b])=>focused>=a&&focused<b);
    showScreenPage(panel,page);
}
function mountScreens(){
  if(!document.createElement)return;
  for(const source of $('page').querySelectorAll('[data-screen-auto]')){
    const children=[...source.children],key=source.dataset.screenAuto,preferred=+source.dataset.screenPreferred;
    if(preferred>=0)screenChoices.set(key,Math.min(preferred,children.length-1));
    source.innerHTML=screenMenu(key,children.map((node,i)=>[node.querySelector('h2,b')?.textContent||`详情 ${i+1}`,'']));
    source.querySelectorAll('.screen-flow').forEach((flow,i)=>flow.append(children[i]));
  }
  for(const panel of $('page').querySelectorAll('.screen-panel')){
    const form=panel.querySelector('form');if(!form||panel.querySelector('.screen-form-actions'))continue;
    const save=form.querySelector('button[data-feature-intent="save"]')||form.querySelector('button[type="submit"]');if(!save)continue;
    const actions=document.createElement('div');actions.className='screen-form-actions';
    save.setAttribute('form',form.id);actions.append(save);
    const test=form.querySelector('[data-test-api]');if(test)actions.append(test);
    panel.querySelector('.screen-pager').before(actions);
  }
  scheduleScreenFit();
  if(!screenObserver&&typeof ResizeObserver==='function'){screenObserver=new ResizeObserver(scheduleScreenFit);screenObserver.observe($('page'))}
}
function chooseScreen(menu,index){
  const panels=[...menu.children].filter(n=>n.classList.contains('screen-panel'));index=Math.max(0,Math.min(panels.length-1,index));
  screenChoices.set(menu.dataset.screenMenu,index);panels.forEach((p,i)=>p.hidden=i!==index);
  menu.querySelectorAll('[data-screen-choice]').forEach((b,i)=>{if(i===index)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current')});
  menu.querySelector('[data-screen-select]').value=String(index);fitScreens();
}
// 键盘翻页：PageUp/PageDown 前后一页，Home/End 到第一页/最后一页。
// 只在当前可见、确实分了多页的面板上生效；没有分页条时返回 false，把按键让给别人。
function screenPageKey(key){
  if(typeof document==='undefined'||!document.querySelectorAll)return false;
  const panel=[...document.querySelectorAll('.screen-panel')].find(p=>!p.hidden&&p._screenRanges&&p._screenRanges.length>1&&p.querySelector('.screen-pager')&&!p.querySelector('.screen-pager').hidden);
  if(!panel)return false;
  const count=panel._screenRanges.length,now=panel._screenPage||0;
  const page=key==='Home'?0:key==='End'?count-1:now+(key==='PageDown'?1:-1);
  if(page===now||page<0||page>=count)return false;
  showScreenPage(panel,page);scheduleScreenFit();return true;
}
function screenClick(e){
  const b=e.target.closest?.('[data-screen-choice],[data-screen-step]');if(!b||(!('screenChoice'in b.dataset)&&!('screenStep'in b.dataset)))return false;
  if(b.dataset.screenChoice!==undefined)chooseScreen(b.closest('.screen-menu'),+b.dataset.screenChoice);
  else{const panel=b.closest('.screen-panel');showScreenPage(panel,(panel._screenPage||0)+Number(b.dataset.screenStep));scheduleScreenFit()}
  return true;
}
window.addEventListener?.('resize',scheduleScreenFit);
document.addEventListener('change',e=>{if(e.target.matches?.('[data-screen-select]'))chooseScreen(e.target.closest('.screen-menu'),+e.target.value)});
document.addEventListener('invalid',e=>{const panel=e.target.closest?.('.screen-panel');if(!panel)return;if(panel.parentElement.classList.contains('screen-menu'))chooseScreen(panel.parentElement,[...panel.parentElement.children].filter(n=>n.classList.contains('screen-panel')).indexOf(panel));else fitScreens();const i=panel._screenUnits.findIndex(n=>n===e.target||n.contains(e.target));const page=panel._screenRanges.findIndex(([a,b])=>i>=a&&i<b);showScreenPage(panel,page)},true);
