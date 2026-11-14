function maintenance(){
  const tabs=['她会做的事','相处的习惯','组件与操作'];
  let html=head('02','她','<span class="act">（把工具箱推过来）</span>她怎么说话、怎么记事，都在这儿。',
      btn('再查一遍','mref',false,state.maintBusy),state.note||'')
    +`<nav class="seg" aria-label="她的设置">${tabs.map((n,i)=>{const on=maintSec===i;return `<button data-maint="${i}" class="${on?'selected':''}"${on?' aria-current="true"':''}>${n}</button>`}).join('')}</nav>`
    +(state.maintBusy?`<div class="panel" role="status" style="margin:0 0 14px"><div class="row"><div><b>正在做：${esc(state.note||'一件事')}</b><small>${esc(state.lastOp||'')}</small></div>${btn('取消','op:cancel')}</div></div>`:'');
  if(maintSec===3)return html+go('外观主题已移到设置',6);
  if(maintSec===2){
    const ops=[['日常',[['重启大脑','restart_brain'],['重登 QQ','relogin'],['重启 QQ','restart_qq'],['自设定','selfsetup'],['一键自动配置','autosetup'],['导入人设','import_skill'],['修开机自启','autostart_fix']]],['组件',[['启动 Breeze','start_breeze'],['启动 Ollama','start_ollama'],['更新技能','skill_update']]],['兜底',[['备份到桌面','backup'],['生成求助信息','diag']]]];
    return html+screenBoard('her-operations',componentStatus()+ops.map(([g,list])=>card(g+'操作','<div class="quick">'+list.map(([n,a])=>btn(n,'op:'+a,false,state.maintBusy)).join('')+'</div>')).join('')+`<p class="dim board-wide" role="status">${esc(state.lastOp)||'动过什么，会写在这里。'}</p>`,'operations-board');
  }
  if(maintSec===1){
    let rows=`<div class="feat"><div class="t"><b>QQ 接入端</b><small>停止当前接入端后切换；SnowLuma 仅作回退。</small></div><div class="opts">${[['hermes','潮信（默认）'],['snowluma','SnowLuma（回退）'],['napcat','NapCat']].map(([v,n])=>`<button data-protocol="${v}" aria-pressed="${(state.protocol||'hermes')===v}"${state.maintBusy?' disabled':''}>${n}</button>`).join('')}</div></div>`;
    rows+=optRow('群里怎么回复','@ 不 @ 引用，随你',[['不 @','freq:group:off','off'],['@ 对方','freq:group:mention','mention'],['引用','freq:group:quote','quote'],['都加','freq:group:both','both']],state.group,state.maintBusy);
    rows+=optRow('表情包','发多少',[['少','freq:meme:few','few'],['适中','freq:meme:mid','mid'],['多','freq:meme:many','many']],state.meme,state.maintBusy);
    rows+=optRow('语音','多久说一次',[['少','freq:voice:few','few'],['适中','freq:voice:mid','mid'],['多','freq:voice:many','many']],state.voiceFreq,state.maintBusy);
    rows+=optRow('掉线提醒','断了就吱一声',[['开','op:watch_on',true],['关','op:watch_off',false],['改间隔','op:watch_every',null],['试一条','op:watch_test',null]],state.watchOn,state.maintBusy);
    rows+=optRow('打游戏模式','打的时候，先让她安静会儿',[[state.gameOn?'打完了，恢复':'开启',state.gameOn?'op:game_off':state.gameCan?'op:game_on':'op:game_hint',true]],state.gameOn,state.maintBusy);
    return html+screenBoard('her-habits',rows,'settings-board');
  }
  const plugins=['astrbot_plugin_dafeiyu_petlink','astrbot_plugin_dafeiyu_sing','astrbot_plugin_seed_audio_tts','astrbot_plugin_breeze_online_tts'];
  const pn=[['桌宠','让桌宠也用她的人设'],['唱歌','想听什么，点一首'],['豆包语音','用豆包的声音'],['在线 Breeze','用在线 Breeze 音色']];
  const capabilities=pn.map((p,i)=>[p[0],card(p[0],optRow(p[0],p[1],[['开','plugin/'+plugins[i]+'/1',true],['关','plugin/'+plugins[i]+'/0',false]],state.features[i],state.maintBusy).replace('</div></div>',`<button data-feature-panel="${['pet','sing','doubao','breeze'][i]}">配置</button></div></div>`))]);
  const memory=optRow('长期记忆','聊过的小事，能记多久',[['关','memory/0',0],['本机 bge-m3','memory/1',1],['硅基流动','memory/2',2]],state.memory,state.maintBusy)+`<p class="dim" role="status">${esc(memoryQueueNote())}</p>`;
  const voice=optRow('语音路线','今天用哪一种声音',[['关','voice/0',0],['Edge','voice/1',1],['豆包','voice/3',3],['本机 Breeze','voice/4',4],['在线 Breeze','voice/5',5]],state.voice,state.maintBusy);
  if(featureConfigOpen)return html+`<div class="feature-back"><button data-feature-back>← 返回能力开关</button></div>`+screenDirect('her-feature-details',featureConfig());
  const overview=capabilities.map(([,body])=>body).join('')+card('长期记忆',memory)+card('语音路线',voice);
  return html+screenBoard('her-features',overview,'capabilities-board');
}
function optRow(title,note,opts,current,busy){
  return `<div class="feat"><div class="t"><b>${esc(title)}</b><small>${esc(note)}</small></div><div class="opts">${opts.map(o=>`<button data-action="${esc(o[1])}" class="${current===o[2]?'selected':''}"${busy?' disabled':''}>${esc(o[0])}</button>`).join('')}</div></div>`;
}
let componentOpen=-1;
function componentStatus(){
  return card('组件状态',`<div class="component-list">${(state.items||[]).map((i,n)=>`<div class="component-row"><button data-component="${n}" aria-expanded="${componentOpen===n}" aria-controls="component-${n}"><b>${esc(i.name)}</b><span class="pill ${/正常|在线|已/.test(i.state)?'':'off'}">${esc(i.state)}</span></button><div id="component-${n}"${componentOpen===n?'':' hidden'}><p>${esc(i.text)}</p><div class="actions">${i.fix?btn(i.fixLabel||'修一下','fix:'+i.fix,false,state.maintBusy):''}${i.linkLabel?btn(i.linkLabel,'lnk:'+n):''}</div></div></div>`).join('')}</div>`);
}

const themePresets={
 dark:{bg:'#101827',panel:'#1b2940',ink:'#e9effc',accent:'#79adff'},
 light:{bg:'#eef3fb',panel:'#ffffff',ink:'#20304a',accent:'#3269d6'}
};
let themeDirty=false;
let featurePanel='sing',featureDirty=false,featureConfigOpen=false;
function featureConfig(){
 const all=state.featureSettings||{},sing=all.sing||{},pet=all.pet||{},doubao=all.doubao||{},breeze=all.breeze||{};
 const disabled=state.maintBusy||all.config_error?' disabled':'';
 const wide=new Set(['root','server_url','breeze_url','breeze_text']);
 const input=(key,label,value,type='text',placeholder='')=>`<label${wide.has(key)?' class="wide"':''}>${esc(label)}<input data-feature-field="${key}" type="${type}" value="${type==='password'?'':esc(value??'')}" placeholder="${esc(placeholder)}"${disabled}></label>`;
 const number=(key,label,value,min,max)=>`<label>${esc(label)}<input data-feature-field="${key}" type="number" min="${min}" max="${max}" value="${value}"${disabled}></label>`;
 const button=(label,action,primary=false)=>`<button type="submit" data-feature-intent="${action}"${primary?' class="primary"':''}${disabled}>${label}</button>`;
 const reset=`<button type="button" data-feature-reset${disabled}>放弃修改</button>`;
 const group=label=>`<p class="group-label">${label}</p>`;
 const credentials=input('app_id','豆包 APP ID',doubao.app_id)+input('token','Access Token','','password',doubao.token_set?'已保存，留空保留':'填写火山引擎 Access Token');
 const tabs=[['sing','唱歌配置'],['pet','桌宠配置'],['doubao','豆包语音'],['breeze','在线 Breeze']];
 let fields='',actions='',note='';
 if(featurePanel==='sing'){
  fields=group('唱歌方式')
   +`<label>走哪条线<select data-feature-field="backend"${disabled}><option value="doubao" ${sing.backend!=='soulx'?'selected':''}>豆包云端</option><option value="soulx" ${sing.backend==='soulx'?'selected':''}>SoulX 本地</option></select></label>`
   +group('豆包云端')+number('tries','最多尝试次数',sing.tries||2,1,5)+credentials
   +group('SoulX 本地')+input('root','本地唱歌安装目录',sing.root||'')+input('server_url','本地服务地址',sing.server_url||'http://127.0.0.1:18620')
   +group('Breeze 参考')+input('breeze_voice','已有 Breeze 音色名称',sing.breeze_voice||'mingling')+input('breeze_url','本机 Breeze 地址',sing.breeze_url||'http://127.0.0.1:8137/v1')
   +input('breeze_text','生成唱歌参考时读的句子',sing.breeze_text||'');
  actions=`<div class="actions">${button('保存设置','save')}${button('保存并启用唱歌','apply',true)}${reset}</div>`
   +`${group('本地服务')}<div class="actions">${button('部署本地服务','install')}${button('启动本地服务','start')}${button('检测所选方式','check')}${button('生成本地试听','test')}</div>`
   +`${group('唱歌参考')}<div class="actions">${button('使用已有冥零参考','voice_existing')}${button('用已有 Breeze 音色制作唱歌参考','voice_breeze')}</div>`;
  note=`两种方式的配置可同时保留。豆包使用云端额度；SoulX 在本机运行。试听生成本机音频，不发送 QQ。\n本地环境：${sing.installed?'已部署':'尚未部署'} · 服务：${sing.running?'正在运行'+(sing.device?' · '+sing.device:''):sing.checking?'检测中':'未就绪'} · 唱歌音色：${sing.voice_name||'待应用冥零参考'}\n使用已有冥零音频，或让 Breeze 以已有音色读参考句，再交给 SoulX 唱歌；不会重新设计音色，也不会生成伴奏。首次制作参考会安装中文对齐工具。`;
 }else if(featurePanel==='pet'){
  fields=input('dsh_home','DSH 数据目录',pet.dsh_home||'')+number('recent_count','带入最近几条消息',pet.recent_count??8,0,50)+number('recent_hours','只带几小时内的消息',pet.recent_hours??12,1,168)
   +`<label>桌宠提醒<select data-feature-field="redirect_reminders" data-feature-type="boolean"${disabled}><option value="true" ${pet.redirect_reminders!==false?'selected':''}>到点发到 QQ 私聊</option><option value="false" ${pet.redirect_reminders===false?'selected':''}>留在桌宠会话</option></select></label>`;
  actions=`<div class="actions">${button('保存设置','save')}${button('保存并启用互通','apply',true)}${button('接通现有桌宠','connect')}${button('检测连接','check')}${reset}</div>`;
  note='先在 DSH 中安装 dsh-pet。接通会配置桌宠桥，让桌宠使用 QQ 中冥零的大脑。';
 }else if(featurePanel==='doubao'){
  fields=credentials;
  actions=`<div class="actions">${button('保存凭据','save')}${button('保存并使用豆包语音','apply',true)}${button('检查凭据','check')}${reset}</div>`;
  note='保存并使用会将语音路线切换为豆包。唱歌与豆包语音共用这套凭据，云端合成消耗账号额度。';
 }else{
  fields=input('key','Breeze API 密钥','','password',breeze.key_set?'已保存，留空保留':'填写在线服务密钥')+input('voice_id','音色 ID',breeze.voice_id||'','text','voc_ 开头');
  actions=`<div class="actions">${button('保存配置','save')}${button('保存并使用在线 Breeze','apply',true)}${button('检查凭据','check')}${reset}</div>`;
  note='使用在线服务的 voc_ 音色 ID。保存并使用会切换语音路线，合成消耗在线账号额度。';
 }
 return card('功能配置',`${all.config_error?`<p role="alert" class="feature-note">${esc(all.config_error)}</p>`:''}<nav class="feature-config-tabs" aria-label="功能配置">${tabs.map(([key,label])=>`<button data-feature-panel="${key}" class="${featurePanel===key?'selected':''}">${label}</button>`).join('')}</nav><p class="dim feature-note">${esc(note)}</p><form id="feature-form" data-kind="${featurePanel}"><div class="feature-fields">${fields}</div><p class="dim">密钥不回显，留空会保留已保存的值。</p><div class="feature-form-foot">${actions}</div></form><p id="feature-feedback" class="feature-note" role="status">${esc(state.lastOp||'')}</p><p id="feature-progress" class="dim feature-note">${esc(all.progress?.text||'')}</p>`);
}
function themeColors(s){
 const defs=themePresets[s.themeBase==='light'?'light':'dark'];
 return Object.fromEntries(Object.entries(defs).map(([k,v])=>[k,/^#[0-9a-f]{6}$/i.test(s['theme_'+k])?s['theme_'+k]:v]));
}
function onTide(hex){
 const n=parseInt(String(hex).slice(1),16);
 const lum=0.299*((n>>16)&255)+0.587*((n>>8)&255)+0.114*(n&255);
 return lum>150?'#101827':'#ffffff';
}
function themeCopy(s){
 if(s.theme==='moon')return {label:'月 轨',mood:'（抬头看月亮）今晚很安静',signature:'冥零 · 月轨'};
 if(s.theme==='suit')return {label:'特 行',mood:'（把领口整了整）该出发了',signature:'冥零 · 特行组'};
 if(s.theme==='daylight')return {label:'她 的 白 昼',mood:'（伸懒腰）晒会儿太阳',signature:'冥零 · 晴日相伴'};
 if(s.theme==='custom')return {label:'她 的 配 色',mood:s.themeBase==='light'?'（眯眼）这配色不错':'（窝好）这配色不错',signature:s.themeBase==='light'?'冥零 · 自选浅色':'冥零 · 自选深色'};
 if(s.theme==='boundary')return {label:'断 界',mood:'（眯眼）看得最清',signature:'冥零 · 断界'};
 return {label:'她 的 夜 晚',mood:'（打哈欠）灯还亮着',signature:'冥零 · 夜色相伴'};
}
function applyTheme(s){
 const root=document.documentElement;if(!root)return;
 root.dataset.style=styleNow(s);
 const avatar=document.querySelector?.('.ava img');if(avatar)avatar.src=CHARACTER_ART.avatar;
 const normalized=canonicalTheme(s.theme); const mode=['daylight','nightfall','boundary','custom','moon','suit'].includes(normalized)?normalized:(normalized==='system'?'':'nightfall');
 if(mode){root.dataset.theme=mode;root.dataset.base=s.themeBase==='light'?'light':'dark'}
 else{delete root.dataset.theme;delete root.dataset.base}
 const colors=themeColors(s);
 for(const [k,v] of [['bg','--bg'],['panel','--panel'],['ink','--ink'],['accent','--tide']]){
   if(mode==='custom')root.style.setProperty(v,colors[k]);else root.style.removeProperty(v);
 }
 if(mode==='custom'){
   root.style.setProperty('--on-tide',onTide(colors.accent));
   root.style.setProperty('--acc',colors.accent);
   root.style.setProperty('--acc-2',colors.accent);
 }else{
   root.style.removeProperty('--on-tide');
   root.style.removeProperty('--acc');
   root.style.removeProperty('--acc-2');
 }
 const copy=themeCopy(s);
 const visual=DIRECTIONS[styleNow(s)];
 const tone={daylight:'白昼',nightfall:'长夜',boundary:'断界',moon:'月轨',suit:'特行'}[canonicalTheme(s.theme)]||'相伴';
 if($('rail-label'))$('rail-label').textContent=visual.name+' · '+tone;
 if($('theme-signature'))$('theme-signature').textContent='冥零 · '+visual.name;
 if($('mood'))$('mood').innerHTML=`${esc(copy.mood)}<small>${esc(navOf(s.tab).hint)}</small>`;
}
// 每个主题给三颗小圆点（底/强调/字），选之前先看一眼是什么颜色。
const THEME_DOTS={"nightfall": ["#151c25", "#9dc7ff", "#f3f6fc"], "daylight": ["#f3f6fa", "#19599b", "#14253b"], "boundary": ["#000000", "#8cffff", "#ffffff"], "moon": ["#0d1627", "#b8cbff", "#f1f4ff"], "suit": ["#f6f4ef", "#a33b0a", "#23272f"]};
function themeDots(key){const d=THEME_DOTS[canonicalTheme(key)];return d?`<span class="theme-dots" aria-hidden="true">${d.map(c=>`<i style="background:${c}"></i>`).join('')}</span>`:''}
function themeSettings(){
 const mode=canonicalTheme(state.theme)||'nightfall';
 const rows=[['system','跟随系统','跟随系统的深浅设置'],['daylight','白昼','清醒的日光与开放空间'],['nightfall','长夜','安静深色，陪伴长夜'],['moon','月轨','银蓝轨迹与夜空'],['suit','特行','行动中的纸白与橙线'],['boundary','断界','清晰边界，正文 AAA'],['custom','自定义','自己配一套颜色']]
   .map(([k,n,d])=>`<label class="theme-pick"><input type="radio" name="theme-pick" value="${k}" ${mode===k?'checked':''} data-theme="${k}"><span><b>${n}</b><small>${d}</small></span>${themeDots(k)}</label>`).join('');
 const colors=themeColors(state),base=state.themeBase==='light'?'light':'dark';
 const sty=styleNow(state);
 const styleRow=`<div class="theme-list">${Object.entries(DIRECTIONS).map(([k,d])=>`<label class="theme-pick"><input type="radio" name="style-pick" value="${k}" ${sty===k?'checked':''}><span><b>${d.name}</b><small>${d.caption}</small></span></label>`).join('')}</div>`;
 const colorRow=(key,label)=>`<label class="theme-color">${label}<input id="theme-${key}" type="color" value="${esc(colors[key])}"></label>`;
 return screenBoard('her-appearance',
 `<div class="board-wide">${card('页面风格',styleRow)}</div>`
 +card('明暗与对比',`<div class="theme-list">${rows}</div>`)
 +card('自定义配色',`<form id="theme-form" class="theme-form">`
  +`<div class="theme-base"><label>自定义的底色<select id="theme-base"><option value="dark" ${base==='dark'?'selected':''}>长夜基底</option><option value="light" ${base==='light'?'selected':''}>白昼基底</option></select></label></div>`
  +`<div class="theme-colors">${colorRow('bg','背景')}${colorRow('panel','面板')}${colorRow('ink','文字')}${colorRow('accent','强调')}</div>`
  +`<div class="actions"><button class="primary" type="submit">应用自定义主题</button></div></form>`),'appearance-board');
}
function memoryQueueNote(){
 const q=state.memoryQueue;
 if(!q||!q.updated||Date.now()/1000-q.updated>120)return '阅读结果在本地排队；启动大脑并启用记忆插件后自动补写。';
 return q.pending?`待写入 ${q.pending} 条${q.error?' · '+q.error:''}`:(q.error||'阅读记忆队列已同步');
}
function saveTheme(mode){
 const colors=themeColors(state);
 for(const k of ['bg','panel','ink','accent']){const el=$('theme-'+k);if(el&&/^#[0-9a-f]{6}$/i.test(el.value))colors[k]=el.value}
 const baseEl=$('theme-base'),base=baseEl&&baseEl.value==='light'?'light':'dark';
 themeDirty=false;force=true;
 send('theme',{value:mode,base,bg:colors.bg,panel:colors.panel,ink:colors.ink,accent:colors.accent,style:styleNow(state)});
 applyTheme({...state,theme:mode,themeBase:base,theme_bg:colors.bg,theme_panel:colors.panel,theme_ink:colors.ink,theme_accent:colors.accent});
}
// 只换风格：主题与四个自定义色原样带回去，后端把 style 存进同一份设置。
function saveStyle(v){
 if(!Object.hasOwn(DIRECTIONS,v))return;
 const mode=state.theme||'system',colors=themeColors(state);
 const base=state.themeBase==='light'?'light':'dark';
 themeDirty=false;force=true;
 state.style=v;
 barrierOpen=state.tab===5&&maintSec===3;barrierTodayOpen=false;
 send('theme',{value:mode,base,bg:colors.bg,panel:colors.panel,ink:colors.ink,accent:colors.accent,style:v});
 applyTheme({...state,style:v});
 render(state);
}

