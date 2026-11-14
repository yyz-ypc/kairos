let settingsSec=0;
function settings(){
 const tabs=['外观主题','模型与 AstrBot','帮助','关于','组件'];
 let html=head('06','设置','外观、连接和帮助都放在这里。')+`<nav class="seg" aria-label="设置分栏">${tabs.map((n,i)=>`<button data-settings="${i}" class="${settingsSec===i?'selected':''}">${n}</button>`).join('')}</nav>`;
 if(settingsSec===0)return html+themeSettings();
 if(settingsSec===1){cfgSec=1;return html+screenMenu('settings-model',[
 ['模型与对话 API',configForm()],
 ['AstrBot',card('AstrBot 配置','<p class="dim">使用 AstrBot 后台配置模型与平台，修改后可重新体检。</p><div class="actions">'+btn('打开 AstrBot 后台','astrbot:open')+btn('自设定','op:selfsetup',false,state.maintBusy)+btn('重新体检','mref',false,state.maintBusy)+'</div>')]
 ]);}
 if(settingsSec===2)return html+help();
 if(settingsSec===4)return html+componentSettings();
 return html+screenMenu('settings-about',[['关于',card(BRAND.name,`<p>${esc(state.version)}</p><p>发布者与版权：${esc(BRAND.name)}</p><p class="dim">这是独立的新项目，角色冥零和现有美术继续保留。</p>` )]]);
}
