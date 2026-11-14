const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const path=require('node:path');
const html=fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
function fixture(){
  const elements={}, handlers={}, messages=[];
  const element=id=>elements[id]??=( {id,children:[],innerHTML:'',textContent:'',value:'',open:false,onclick:null,
    matches:()=>false,blur(){document.activeElement={matches:()=>false}},showModal(){this.open=true},close(){this.open=false}} );
  const document={getElementById:id=>elements[id]||null,activeElement:{matches:()=>false},addEventListener:(name,fn)=>handlers[name]=fn};
  const styles=new Map();
  document.documentElement={dataset:{},style:{setProperty:(k,v)=>styles.set(k,v),removeProperty:k=>styles.delete(k)}};
  for(const id of ['nav','clock','workspace','connection','side-version','crumb','toast','notification','page','editor','save-ask','cancel-ask','ask-text'])element(id);
  const context=vm.createContext({document,window:{chrome:{webview:{postMessage:m=>messages.push(m),addEventListener(){}}},scrollTo(){}},setTimeout(){},console});
  vm.runInContext(script,context);
  const state={version:'V1.0.0',wizardComplete:true,style:'seer',tab:0,bsec:0,clock:'2026-11-07 20:00:00',workspace:'test-workspace',toast:'',stats:['0','0','0','0'],services:[{name:'AstrBot',desc:'对话',port:'6199',up:false,pid:0}],jobs:[],feed:[],items:[],features:[false,false,false,false],docs:[{title:'教程',hint:'离线',id:'guide'}],docsel:-1,asks:[{title:'帮助',body:'测试'}],updates:[],logsrc:0,check:'未检查',input:'',watchBusy:false,progress:0};
  return {context,elements,handlers,messages,element,state,styles,render:s=>{context.fixtureState=s;vm.runInContext('render(fixtureState)',context)}};
}
test('all eight pages render without a backend or script errors',()=>{
  const f=fixture();assert.equal(f.messages[0].cmd,'ready');
  for(let tab=0;tab<8;tab++){f.render({...f.state,tab});assert.match(f.elements.page.innerHTML,/<h1>/);}
  for(let bsec=0;bsec<4;bsec++){f.render({...f.state,tab:1,bsec});assert.match(f.elements.page.innerHTML,/一起看看/);}
});
test('tide page renders hosted, unhosted and offline states, and escapes message text',()=>{
  const f=fixture(),bad='<img src=x onerror="alert(1)">';
  const tide={reachable:true,managed:true,error:'',opMsg:'',opOk:true,busy:false,
    state:{name:'潮信 TideLetter',version:'0.2.0',uptime:'1 分钟',accounts:[{uin:'10001',nickname:'小号',online:true}],networks:[],bridge:{url:'ws://127.0.0.1:6199/ws'},native:{available:true,running:false},settings:{theme:'system',accent:'#1f7a78',notes:'',logLevel:'INFO',logRetainDays:14,reducedMotion:false}},
    logs:[{time:'2026-11-11 22:00:00',level:'INFO',scope:'webui',message:bad}],
    messages:{'10001':[{payload:{sender:{nickname:bad,user_id:'2'},message:bad,time:1},historical:false,saved:''}]}};
  f.render({...f.state,tab:7,tide});let h=f.elements.page.innerHTML;
  assert.match(h,/潮信/);assert.ok(!h.includes('还没设置密码'));assert.ok(!h.includes('tide-pwd-form'));assert.ok(!h.includes(bad));
  // 没账号、QQ 开着但没接管时，要给出可点的接管路径，而不是只说「到 QQ 里扫码」
  const idle={...tide,state:{...tide.state,accounts:[],native:{available:true,running:false,qqRunning:true}}};
  f.render({...f.state,tab:7,tide:idle});h=f.elements.page.innerHTML;
  assert.match(h,/QQ 正开着/);assert.match(h,/退出 QQ 并接管/);assert.match(h,/tide_takeover_ask/);
  assert.ok(!h.includes('还没有账号登录'));assert.ok(!h.includes('tide-pwd-form'));
  const off={...tide,state:{...tide.state,accounts:[],native:{available:true,running:false,qqRunning:false}}};
  f.render({...f.state,tab:7,tide:off});h=f.elements.page.innerHTML;
  assert.match(h,/用潮信启动 QQ/);assert.match(h,/tide:native_start/);
  const live={...tide,state:{...tide.state,accounts:[],native:{available:true,running:true,qqRunning:true,runs:[{id:'a1b2c3d4-0000-4000-8000-000000000000',pid:4242,alive:true,stage:'dll_loaded'}]}}};
  f.render({...f.state,tab:7,tide:live});h=f.elements.page.innerHTML;
  assert.match(h,/原生入口打开了 QQ/);assert.match(h,/4242/);
  f.handlers.click({target:{closest:()=>({dataset:{tide:'3'}})}});h=f.elements.page.innerHTML;
  assert.match(h,/已由潮信托管/);assert.match(h,/停止注入/);
  f.render({...f.state,tab:3,tide});h=f.elements.page.innerHTML;
  assert.ok(!h.includes(bad));assert.match(h,/&lt;img/);
  f.render({...f.state,tab:7,tide:{...tide,reachable:false,managed:false}});
  assert.match(f.elements.page.innerHTML,/启动潮信/);
  f.render({...f.state,tab:7,tide:{...tide,reachable:true,managed:false}});
  assert.match(f.elements.page.innerHTML,/重启/);assert.match(f.elements.page.innerHTML,/别的程序启动/);
});
test('backend text is escaped before entering HTML, including result and logs',()=>{
  const f=fixture(),payload='<img src=x onerror="alert(1)">';
  f.render({...f.state,tab:3,log:payload});
  assert.ok(!f.elements.page.innerHTML.includes(payload));assert.match(f.elements.page.innerHTML,/&lt;img/);
  f.render({...f.state,tab:1,bsec:2,resultOk:true,result:payload,resultTitle:payload});
  assert.ok(!f.elements.page.innerHTML.includes(payload));
});
test('live pushes preserve a focused draft while still updating status and toasts',()=>{
  const f=fixture();f.render(f.state);const before=f.elements.page.innerHTML;
  f.context.document.activeElement={matches:()=>true};
  f.render({...f.state,stats:['9','1','0','0'],toast:'完成',clock:'later'});
  assert.equal(f.elements.page.innerHTML,before);assert.equal(f.elements.toast.textContent,'完成');assert.equal(f.elements.clock.textContent,'later');
});
test('live pushes preserve unsaved configuration fields',()=>{
  const f=fixture();f.render({...f.state,tab:1,bsec:3});const before=f.elements.page.innerHTML;
  f.element('config-form');f.render({...f.state,tab:1,bsec:3,configNote:'连接成功',stats:['8','0','0','0']});
  assert.equal(f.elements.page.innerHTML,before);
});

test('API test submits the current draft without saving or clearing the key',()=>{
  const f=fixture();
  const secret={dataset:{row:'12'},type:'password',value:'draft-key',defaultValue:''};
  const fields=[{dataset:{row:'8'},type:'text',value:'https://draft.test/v1',defaultValue:''},secret];
  f.element('config-form').querySelectorAll=()=>fields;
  f.handlers.click({target:{closest:()=>({dataset:{testApi:''}})}});
  const m=f.messages.at(-1);
  assert.equal(m.cmd,'configtest');assert.equal(m.value,'link');assert.equal(m.fields[0].value,'https://draft.test/v1');
  assert.equal(m.fields[1].value,'draft-key');assert.equal(secret.value,'draft-key');
  assert.ok(!f.messages.some(m=>m.cmd==='configsave'));
});

test('API test status updates without replacing the unsaved form',()=>{
  const f=fixture();f.render({...f.state,tab:1,bsec:3});const before=f.elements.page.innerHTML;
  f.element('config-form');f.element('config-feedback');f.element('api-test');
  f.render({...f.state,tab:1,bsec:3,configNote:'正在测试连接…',apiTestBusy:true});
  assert.equal(f.elements['api-test'].disabled,true);
  f.render({...f.state,tab:1,bsec:3,configNote:'连上了：你好',apiTestBusy:false});
  assert.equal(f.elements['config-feedback'].textContent,'连上了：你好');
  assert.equal(f.elements['api-test'].disabled,false);assert.equal(f.elements.page.innerHTML,before);
});

test('feed shows failures and offers API scheduling with escaped plan data',()=>{
  const f=fixture();f.render({...f.state,tab:1,bsec:0,feedNote:'刷新失败：请登录',biliAgent:{note:'等待',tasks:[{title:'<unsafe>',reason:'<reason>',due:100,status:'pending'}]}});
  const page=f.elements.page.innerHTML;
  assert.match(page,/刷新失败：请登录/);assert.match(page,/&lt;unsafe&gt;/);assert.match(page,/&lt;reason&gt;/);
  assert.match(page,/data-bili-agent="once"/);
  f.handlers.click({target:{closest:()=>({dataset:{biliAgent:'once'}})}});
  assert.equal(f.messages.at(-1).cmd,'biliagent');assert.equal(f.messages.at(-1).value,'once');
});
test('typing digits does not trigger navigation; page keyboard navigation has typed parameters',()=>{
  const f=fixture();let prevented=false;
  f.handlers.keydown({key:'3',target:{matches:()=>true}});assert.equal(f.messages.length,1);
  f.handlers.keydown({key:'3',target:{matches:()=>false},preventDefault(){prevented=true}});
  assert.equal(f.messages.at(-1).cmd,'tab');assert.equal(f.messages.at(-1).index,3);assert.equal(prevented,true);
});
test('configuration submit sends only editable fields and clears entered secrets',()=>{
  const f=fixture();const secret={dataset:{row:'2'},type:'password',value:'test-secret',defaultValue:'',readOnly:false};
  const fields=[{dataset:{row:'1'},type:'checkbox',checked:true,defaultChecked:false,readOnly:false},secret,{dataset:{row:'4'},value:'readonly',readOnly:true}];
  const form={id:'config-form',dataset:{kind:'link'},querySelectorAll:selector=>selector.includes('password')?[secret]:fields};
  f.handlers.submit({target:form,preventDefault(){}});
  const m=f.messages.at(-1);assert.equal(m.cmd,'configsave');assert.equal(m.fields.length,2);assert.equal(m.fields[0].value,'开');assert.equal(secret.value,'');
});

test('unchanged fields are not submitted as edits',()=>{
  const f=fixture();
  const fields=[{dataset:{row:'1'},type:'text',value:'original',defaultValue:'original'}, {dataset:{row:'2'},type:'checkbox',checked:true,defaultChecked:true}];
  f.handlers.submit({target:{id:'config-form',dataset:{kind:'bili'},querySelectorAll:s=>s.includes('password')?[]:fields},preventDefault(){}});
  assert.equal(f.messages.at(-1).fields.length,0);
});

test('five explicit themes plus follow-system drive the document theme attribute',()=>{
  const f=fixture();
  for(const theme of ['nightfall','daylight','boundary','moon','suit']){
    f.render({...f.state,theme});
    assert.equal(f.context.document.documentElement.dataset.theme,theme);
  }
  f.render({...f.state,theme:'system'});
  assert.equal(f.context.document.documentElement.dataset.theme,undefined);
});
test('picking an appearance theme sends it through the native settings bridge',()=>{
  const f=fixture();f.render({...f.state,tab:5});
  f.handlers.click({target:{closest:()=>({dataset:{settings:'0'}})}});
  const radio={matches:q=>q==='input[name=theme-pick]',value:'daylight',closest:()=>null};
  f.handlers.click({target:radio});
  const m=f.messages.at(-1);
  assert.equal(m.cmd,'theme');assert.equal(m.value,'daylight');
});

test('her page puts the switches first and keeps operations and appearance reachable',()=>{
  const f=fixture();
  f.render({...f.state,tab:5,items:[{name:'AstrBot',state:'离线',text:'未启动',fix:'brain',fixLabel:'启动',linkLabel:'打开后台'}],lastOp:'上一次已完成'});
  let page=f.elements.page.innerHTML;
  assert.deepEqual([...page.matchAll(/data-maint="\d"[^>]*>([^<]+)</g)].map(m=>m[1]),['她会做的事','相处的习惯','组件与操作']);
  assert.match(page,/语音路线/);assert.match(page,/长期记忆/);
  f.handlers.click({target:{closest:()=>({dataset:{maint:'2'}})}});
  page=f.elements.page.innerHTML;
  assert.match(page,/日常操作/);assert.match(page,/兜底操作/);assert.match(page,/组件状态/);assert.match(page,/AstrBot/);assert.match(page,/上一次已完成/);
  for(const action of ['op:selfsetup','op:autosetup','op:backup','fix:brain','lnk:0'])assert.ok(page.includes(`data-action="${action}"`),action);
  f.handlers.click({target:{closest:()=>({dataset:{maint:'1'}})}});
  assert.match(f.elements.page.innerHTML,/群里怎么回复/);assert.doesNotMatch(f.elements.page.innerHTML,/data-action="op:selfsetup"/);
  f.handlers.click({target:{closest:()=>({dataset:{settings:'0'}})}});
  f.render({...f.state,tab:6});assert.match(f.elements.page.innerHTML,/跟随系统|外观/);
});

test('combined maintenance keeps actions and fixes disabled while busy',()=>{
  const f=fixture();
  f.render({...f.state,tab:5,maintBusy:true,items:[{name:'AstrBot',state:'离线',text:'未启动',fix:'brain'}]});
  f.handlers.click({target:{closest:()=>({dataset:{maint:'2'}})}});
  const page=f.elements.page.innerHTML;
  assert.match(page,/<button[^>]*data-action="op:selfsetup"[^>]*disabled/);
  assert.match(page,/<button[^>]*data-action="fix:brain"[^>]*disabled/);
});

test('a running maintenance step can be cancelled from the page',()=>{
  const f=fixture();
  f.render({...f.state,tab:5,maintBusy:true,note:'正在做：自设定'});
  const page=f.elements.page.innerHTML;
  assert.match(page,/正在做：自设定/);
  assert.match(page,/<button[^>]*data-action="op:cancel"/);
  assert.ok(!/<button[^>]*data-action="op:cancel"[^>]*disabled/.test(page));
});

test('feature configuration exposes both singing routes and local deployment controls',()=>{
  const f=fixture();f.render({...f.state,tab:5,featureSettings:{sing:{backend:'soulx',root:'F:\\唱歌',installed:true,running:false},doubao:{token_set:true}}});
  f.handlers.click({target:{closest:()=>({dataset:{featurePanel:'sing'}})}});
  const page=f.elements.page.innerHTML;
  assert.match(page,/value="soulx" selected/);assert.match(page,/豆包云端/);
  for(const intent of ['save','apply','install','start','check','test','voice_existing','voice_breeze'])assert.ok(page.includes(`data-feature-intent="${intent}"`));
  assert.match(page,/data-feature-field="breeze_voice"/);
  assert.match(page,/data-feature-field="breeze_text"/);
  assert.match(page,/已保存，留空保留/);assert.doesNotMatch(page,/type="password" value="[^"\s]+/);
});

test('feature submit keeps types, routes the selected action and clears secret input',()=>{
  const f=fixture(),secret={dataset:{featureField:'token'},type:'password',value:'fixture-token'};
  const fields=[{dataset:{featureField:'backend'},type:'select-one',value:'soulx'},
    {dataset:{featureField:'tries'},type:'number',value:'3'},secret,
    {dataset:{featureField:'emptySecret'},type:'password',value:''}];
  f.handlers.submit({target:{id:'feature-form',dataset:{kind:'sing'},querySelectorAll:s=>s.includes('password')?[secret]:fields},submitter:{dataset:{featureIntent:'install'}},preventDefault(){}});
  const m=f.messages.at(-1);
  assert.equal(m.cmd,'featureconfig');assert.equal(m.action,'install');assert.equal(m.kind,'sing');
  assert.equal(m.fields.tries,3);assert.equal(m.fields.backend,'soulx');assert.equal(m.fields.token,'fixture-token');
  assert.ok(!('emptySecret'in m.fields));assert.equal(secret.value,'');
});

test('all feature forms render without changing the current voice route',()=>{
  const f=fixture();f.render({...f.state,tab:5,voice:4});
  for(const kind of ['pet','doubao','breeze','sing']){
    f.handlers.click({target:{closest:()=>({dataset:{featurePanel:kind}})}});
    assert.ok(f.elements.page.innerHTML.includes(`data-kind="${kind}"`));
    assert.equal(f.messages.length,1);
  }
});

test('damaged feature configuration is visible and prevents overwriting from the form',()=>{
  const f=fixture();f.render({...f.state,tab:5,featureSettings:{config_error:'配置损坏 <保留原文件>'}});
  f.handlers.click({target:{closest:()=>({dataset:{featurePanel:'sing'}})}});
  const page=f.elements.page.innerHTML;
  assert.match(page,/role="alert"/);assert.match(page,/&lt;保留原文件&gt;/);
  assert.match(page,/<button[^>]*data-feature-intent="save"[^>]*disabled/);
  assert.match(page,/<button[^>]*data-feature-intent="voice_breeze"[^>]*disabled/);
});

test('today page no longer advertises the ecosystem panel',()=>{
  const f=fixture();f.render({...f.state,tab:0});
  const page=f.elements.page.innerHTML;
  assert.ok(!page.includes('项目生态'),'今日页不该再有项目生态卡片');
  assert.ok(!page.includes('proj:'),'今日页不该再发 proj: 指令');
  assert.ok(!page.includes('时序之子'),'今日页不该再出现时序之子入口');
  assert.match(page,/最近/, '最近记录应出现在右栏');
});

test('today shows input and three clickable recent records directly',()=>{
  const f=fixture();f.render({...f.state,jobs:Array.from({length:5},(_,i)=>({title:`记录 ${i}`,status:'完成',time:1}))});
  const h=f.elements.page.innerHTML;
  assert.match(h,/today-screen/);assert.match(h,/screen-board dashboard-board/);assert.doesNotMatch(h,/data-screen-menu="today"/);assert.doesNotMatch(h,/<dialog/);
  assert.equal((h.match(/id="watch-input"/g)||[]).length,1);assert.equal((h.match(/class="recent-entry"/g)||[]).length,3);
  assert.doesNotMatch(h,/卡住了就喊我|class="cols"/);assert.match(h,/data-nav="4"/);
  f.handlers.click({target:{closest:()=>({dataset:{nav:'5',maintGo:'2'}})}});
  f.render({...f.state,tab:5});assert.match(f.elements.page.innerHTML,/组件状态/);
});
test('component detail toggles from keyboard button semantics and Escape collapses it',()=>{
  const f=fixture();f.render({...f.state,tab:5,items:[{name:'潮信',state:'正常',text:'说明',fix:'qq'}]});
  f.handlers.click({target:{closest:()=>({dataset:{maint:'2'}})}});
  assert.match(f.elements.page.innerHTML,/data-component="0" aria-expanded="false"/);
  f.handlers.click({target:{closest:()=>({dataset:{component:'0'}})}});
  assert.match(f.elements.page.innerHTML,/data-component="0" aria-expanded="true"/);
  f.handlers.keydown({key:'Escape',target:{matches:()=>false}});
  assert.match(f.elements.page.innerHTML,/data-component="0" aria-expanded="false"/);
});
test('explicit QQ fallback choice uses a typed native command and backend whitelist',()=>{
  const f=fixture();f.handlers.click({target:{closest:()=>({dataset:{protocol:'snowluma'}})}});
  assert.equal(f.messages.at(-1).cmd,'protocol');assert.equal(f.messages.at(-1).value,'snowluma');
  const native=fs.readFileSync(path.join(__dirname,'../src/ui_web.inc'),'utf8');
  assert.match(native,/cmd == L"protocol"/);assert.match(native,/value == BRAND_HERMES \|\| value == L"snowluma" \|\| value == L"napcat"/);
});
test('native display names, startup migration, window title and encoding gates stay wired',()=>{
  const read=n=>fs.readFileSync(path.join(__dirname,'../../',n),'utf8');
  assert.match(read('win32/src/her/her_actions.cpp'),/q.label = L"潮信"/);
  assert.match(read('win32/src/her/her_status.cpp'),/L"潮信后台"/);
  assert.match(read('win32/src/her/her_actions.cpp'),/Convert-TideStartupName \$startup \$Dir/);
  assert.doesNotMatch(read('tools/autostart.ps1').split('function Set-OllamaAutostart')[0],/Move-Item|Remove-Item/);
  assert.match(read('win32/src/main.cpp'),/WIN_TITLE = APP_NAME L" · " APP_VERSION/);
  assert.match(read('win32/build.bat'),/check-encoding.ps1" -VerifyExe/);
  assert.match(read('win32/src/ui_pages.inc'),/href == \(L"http:\/\/127.0.0.1:" \+ std::to_wstring\(HermesPort\(\)\)/);
  assert.match(read('win32/src/her/her_actions.cpp'),/请在工作台「潮信」页停止并重新接入 QQ/);
  assert.match(read('win32/src/ui_pages.inc'),/DWORD pid = her::TideLaunch\(&error\)/);
  assert.match(read('win32/src/ui_pages.inc'),/if \(her::Protocol\(\) != BRAND_HERMES\) return LogPath\(0\)/);
});
test('takeover from bridge view shows its confirmation and Escape cancels it',()=>{
  const f=fixture(),tide={reachable:true,managed:true,state:{accounts:[],native:{available:true,qqRunning:true},networks:[{name:'astrbot',status:'reconnecting',error:'UNAUTHORIZED',address:'ws://127.0.0.1:6199/ws'}]}};
  f.render({...f.state,tab:7,tide});f.handlers.click({target:{closest:()=>({dataset:{tide:'3'}})}});
  assert.match(f.elements.page.innerHTML,/不是 AstrBot OneBot 地址/);assert.match(f.elements.page.innerHTML,/鉴权失败/);
  f.handlers.click({target:{closest:()=>({dataset:{action:'tide_takeover_ask'}})}});
  assert.match(f.elements.page.innerHTML,/接管前确认/);
  f.handlers.keydown({key:'Escape',target:{matches:()=>false}});assert.doesNotMatch(f.elements.page.innerHTML,/接管前确认/);
});
test('five structures render six pages in five themes with identical DOM within each structure',()=>{
  let cases=0;
  const wrap={seer:'seer-(today|workspace)',array:'array-(today|workspace)',barrier:'barrier-(today|workspace|public)',tower:'tower-(today|workspace)',folio:'folio-(today|workspace)'};
  for(const style of ['seer','array','barrier','tower','folio'])for(const tab of [0,5,7,2,4,6]){
    const f=fixture();vm.runInContext("settingsSec=3",f.context);let before;
    for(const theme of ['nightfall','daylight','boundary','moon','suit']){
      f.render({...f.state,style,theme,tab,tide:{reachable:true,managed:true,state:{accounts:[],native:{},networks:[]}}});
      const h=f.elements.page.innerHTML;
      assert.equal(f.context.document.documentElement.dataset.style,style);assert.equal(f.context.document.documentElement.dataset.theme,theme);
      assert.match(h,/<h1>/);assert.match(h,new RegExp(wrap[style]));
      if(before)assert.equal(h,before,`${style}/${tab}: changing only the theme must not change structure`);
      before=h;cases++;
    }
  }
  assert.equal(cases,150);
});
test('today preserves the directions with a directly visible overview',()=>{
  const f=fixture(),outputs=[];
  for(const style of ['seer','array','barrier','tower','folio']){f.render({...f.state,style});outputs.push(f.elements.page.innerHTML)}
  assert.match(outputs[0],/seer-today/);assert.match(outputs[0],/此刻 · 前景/);
  assert.match(outputs[1],/array-today/);assert.match(outputs[1],/外环 · 灵引/);assert.match(outputs[1],/dependency-board/);
  assert.match(outputs[2],/barrier-today/);assert.match(outputs[2],/她身边/);
  assert.match(outputs[3],/tower-today/);assert.match(outputs[4],/folio-today/);
  assert.equal(new Set(outputs).size,5);
});
test('barrier omits private account details until opened and Escape hides them again',()=>{
  const f=fixture();f.render({...f.state,style:'barrier',tab:7,tide:{reachable:true,managed:true,state:{accounts:[{uin:'10000002',online:true}],native:{},networks:[]}}});
  assert.ok(!f.elements.page.innerHTML.includes('10000002'));
  f.handlers.click({target:{closest:()=>({dataset:{gate:'page'}})}});assert.ok(f.elements.page.innerHTML.includes('10000002'));
  f.handlers.keydown({key:'Escape',target:{matches:()=>false},preventDefault(){}});assert.ok(!f.elements.page.innerHTML.includes('10000002'));
});
test('closing the gate preserves an unsaved feature or tide form',()=>{
  const f=fixture();f.render({...f.state,style:'barrier',tab:5});
  f.handlers.click({target:{closest:()=>({dataset:{gate:'page'}})}});
  const before=f.elements.page.innerHTML;f.element('gate-feedback');
  vm.runInContext('featureDirty=true',f.context);
  f.handlers.click({target:{closest:()=>({dataset:{gate:'page'}})}});
  assert.equal(f.elements.page.innerHTML,before);assert.match(f.elements['gate-feedback'].textContent,/未保存/);
  vm.runInContext('featureDirty=false;gateFormDirty=true',f.context);
  f.handlers.keydown({key:'Escape',target:{matches:()=>true},preventDefault(){}});
  assert.equal(f.elements.page.innerHTML,before);
});
test('new style options preserve theme and colors through the native persistence command',()=>{
  const f=fixture();f.render({...f.state,tab:6,theme:'custom',themeBase:'light',theme_bg:'#ffffff',theme_panel:'#fafafa',theme_ink:'#111111',theme_accent:'#006b70'});
  f.handlers.click({target:{closest:()=>({dataset:{settings:'0'}})}});
  for(const style of ['seer','array','barrier','tower','folio']){
    const radio={matches:q=>q==='input[name=style-pick]',value:style,closest:()=>null};
    f.handlers.click({target:radio});const m=f.messages.at(-1);
    assert.equal(m.cmd,'theme');assert.equal(m.style,style);assert.equal(m.value,'custom');assert.equal(m.bg,'#ffffff');assert.equal(m.accent,'#006b70');
  }
  for(const v of ['seer','array','barrier','tower','folio'])assert.match(f.elements.page.innerHTML,new RegExp(`value="${v}"`));
  assert.doesNotMatch(f.elements.page.innerHTML,/value="ledger"|value="watch"/);
  const native=fs.readFileSync(path.join(__dirname,'../src/ui_web.inc'),'utf8');
  assert.match(native,/sty == L"tower" \|\| sty == L"folio"/);
});
test('legacy style values fall back to barrier and old visual selectors are removed',()=>{
  const f=fixture();for(const style of ['ledger','watch','unknown']){f.render({...f.state,style});assert.equal(f.context.document.documentElement.dataset.style,'barrier')}
  const css=['tokens.css','base.css','components.css','directions.css'].map(n=>fs.readFileSync(path.join(__dirname,'../web',n),'utf8')).join('\n');
  assert.doesNotMatch(css,/data-style\s*=\s*["']?(?:ledger|watch)/);
  assert.match(css,/@media\s*\(max-width:979px\)/);assert.match(css,/@media\s*\(max-width:860px\)/);assert.match(css,/@media\s*\(max-width:760px\)/);
});
test('radio focus restoration distinguishes style options after a full redraw',()=>{
  const f=fixture();let focused='';
  const old={tagName:'INPUT',type:'radio',name:'style-pick',value:'array',matches:()=>true};
  const inputs=['seer','array','barrier','tower','folio'].map(value=>({tagName:'INPUT',type:'radio',name:'style-pick',value,focus(){focused=value}}));
  f.context.document.activeElement=old;f.context.document.querySelectorAll=()=>inputs;
  vm.runInContext('captureFocus();restoreFocus()',f.context);assert.equal(focused,'array');
});

test('all route families use bounded panels without a scrolling drawer',()=>{
  const f=fixture();
  for(const style of ['seer','array','barrier','tower','folio']){
    vm.runInContext('barrierOpen=true',f.context);
    for(let tab=0;tab<8;tab++){
      f.render({...f.state,style,tab});
      if(style==='barrier'&&[5,7].includes(tab))f.handlers.click({target:{closest:()=>({dataset:{gate:'page'}})}});
      assert.match(f.elements.page.innerHTML,/screen-panel|data-screen-auto=/,`${style} route ${tab}`);
      assert.doesNotMatch(f.elements.page.innerHTML,/<dialog id="today-side"|data-tail=/);
    }
  }
});
test('pagination keeps every item reachable, fits the available height and keeps headings with fields',()=>{
  const f=fixture();
  f.context.heights=Array.from({length:101},(_,i)=>24+(i*17)%70);
  for(const capacity of [140,215,320,460]){
    f.context.capacity=capacity;
    const pages=JSON.parse(vm.runInContext('JSON.stringify(screenRanges(heights,capacity))',f.context));
    const all=[];
    for(const [a,b] of pages){all.push(...Array.from({length:b-a},(_,i)=>a+i));assert.ok(f.context.heights.slice(a,b).reduce((s,h)=>s+h,0)+8*(b-a-1)<=capacity)}
    assert.deepEqual(all,Array.from({length:101},(_,i)=>i));
  }
  assert.equal(vm.runInContext('JSON.stringify(screenRanges([80,20,70],150,8,[false,true]))',f.context),'[[0,1],[1,3]]');
});
test('switching a secondary menu preserves the actual draft nodes and entered secrets',()=>{
  const f=fixture(),secret={value:'session-only-key'},panels=[0,1].map(i=>({id:'p'+i,hidden:i!==0,classList:{contains:n=>n==='screen-panel'},secret})),buttons=[0,1].map(i=>({setAttribute(k,v){this[k]=v},removeAttribute(k){delete this[k]}})),select={value:'0'};
  const menu={dataset:{screenMenu:'draft-test'},children:panels,querySelectorAll:()=>buttons,querySelector:()=>select};
  f.context.menu=menu;
  vm.runInContext('chooseScreen(menu,1)',f.context);assert.equal(panels[0].hidden,true);assert.equal(panels[1].hidden,false);
  vm.runInContext('chooseScreen(menu,0)',f.context);assert.equal(panels[0].hidden,false);assert.equal(panels[0].secret,secret);assert.equal(secret.value,'session-only-key');assert.equal(select.value,'0');assert.equal(buttons[0]['aria-current'],'page');
});
test('measured flow refits on resize and exposes every row through its pager',()=>{
  const f=fixture(),label={},prev={},next={};
  const units=Array.from({length:13},(_,i)=>({nodeType:1,tagName:'DIV',hidden:false,dataset:{},textContent:'row '+i,matches:()=>false,contains:()=>false,getBoundingClientRect:()=>({height:40})}));
  const flow={clientHeight:220,childNodes:units,querySelectorAll:()=>units};
  const pager={querySelectorAll:()=>[prev,next],querySelector:()=>label};
  const panel={id:'test-flow',closest:()=>null,querySelector:s=>s==='.screen-flow'?flow:pager};
  f.elements.page.querySelectorAll=()=>[panel];f.context.document.querySelectorAll=()=>[];f.context.document.createElement=()=>({});
  f.context.panel=panel;vm.runInContext('fitScreens()',f.context);
  assert.equal(panel._screenRanges.length,4);assert.equal(prev.disabled,true);assert.equal(label.textContent,'第 1 / 4 页');
  const seen=[];for(let i=0;i<4;i++){f.context.pageIndex=i;vm.runInContext('showScreenPage(panel,pageIndex)',f.context);seen.push(...units.filter(n=>!n.hidden).map(n=>n.textContent))}
  assert.deepEqual(seen,units.map(n=>n.textContent));assert.equal(next.disabled,true);
  flow.clientHeight=420;vm.runInContext('fitScreens()',f.context);assert.equal(panel._screenRanges.length,2);
});
test('editing segments preserve emoji, newlines, whitespace and the full message',()=>{
  const f=fixture(),text=('中文🙂\r\n  换行\n\n尾部\t'+'很长的内容'.repeat(80)).repeat(3);
  f.context.fullText=text;
  for(const limit of [24,48,180]){
    f.context.limit=limit;const parts=JSON.parse(vm.runInContext('JSON.stringify(textSlices(fullText,limit))',f.context));
    assert.equal(parts.join(''),text);for(const part of parts){assert.ok(Array.from(part).length<=limit);assert.ok(part.split('\n').length<=4);assert.doesNotMatch(part,/^[\uDC00-\uDFFF]|[\uD800-\uDBFF]$/)}
  }
});
test('help chapters ignore code headings and retain the full article',()=>{
  const f=fixture(),doc='开篇\n# 安装\n说明\n```ps1\n# 这是代码注释\n```\n## 检查\n结束';f.context.doc=doc;
  const chapters=JSON.parse(vm.runInContext('JSON.stringify(helpChapters(doc))',f.context));
  assert.deepEqual(chapters.map(x=>x[0]),['正文','安装','检查']);assert.equal(chapters.map(x=>x[1]).join('\n'),doc);
});
test('long QQ messages and all received log entries remain in the paginated source',()=>{
  const f=fixture(),body='长消息🙂'.repeat(200)+'完整结尾',logs=Array.from({length:95},(_,i)=>({message:'日志'+i,level:'INFO'}));
  f.render({...f.state,tab:3,tide:{logs}});assert.match(f.elements.page.innerHTML,/日志0</);assert.match(f.elements.page.innerHTML,/日志94</);
  vm.runInContext('tideSec=2',f.context);
  f.render({...f.state,tab:7,tide:{reachable:true,managed:true,state:{accounts:[{uin:'10001'}]},messages:{'10001':[{payload:{message:body}}]}}});
  assert.ok(f.elements.page.innerHTML.includes(body));
});
test('a long notification opens complete paginated details instead of a tall overlay',()=>{
  const f=fixture(),notify={title:'需要查看',body:'通知内容'.repeat(500)+'结尾'};
  f.render({...f.state,notify});assert.ok(!f.elements.notification.innerHTML.includes(notify.body));assert.match(f.elements.notification.innerHTML,/data-notice-details/);
  f.handlers.click({target:{closest:()=>({dataset:{noticeDetails:''}})}});assert.equal(f.messages.at(-1).cmd,'tab');assert.equal(f.messages.at(-1).index,4);
  f.render({...f.state,tab:4,notify});assert.ok(f.elements.page.innerHTML.includes(notify.body));assert.match(f.elements.page.innerHTML,/screen-diagnostics-2" class="screen-panel" aria-label="通知详情">/);
});
test('splitting an oversized text block keeps all characters and each measured fragment fits',()=>{
  const f=fixture(),text='开始🙂'+('很长的一行\n'.repeat(40))+'尾部🙂';
  function node(value){const leaf={textContent:value};return {leaf,get textContent(){return leaf.textContent},cloneNode(){return node(leaf.textContent)},removeAttribute(){},before(){},append(){},remove(){},getBoundingClientRect(){return {height:Math.ceil(Array.from(leaf.textContent).length/12)*20+20}},replaceChildren(...children){this.children=children},classList:{add(){}}}}
  f.context.document.createTreeWalker=clone=>{let used=false;return {nextNode(){if(used)return null;used=true;return clone.leaf}}};
  f.context.NodeFilter={SHOW_TEXT:4};const original=node(text);f.context.original=original;
  assert.equal(vm.runInContext('splitScreenText(original,100)',f.context),true);
  assert.equal(original.children.map(n=>n.textContent).join(''),text);
  for(const part of original.children){assert.ok(part.getBoundingClientRect().height<=100);assert.doesNotMatch(part.textContent,/^[\uDC00-\uDFFF]|[\uD800-\uDBFF]$/)}
});
test('live state updates preserve a blurred unsent QQ draft',()=>{
  const f=fixture();f.render({...f.state,tab:7});const before=f.elements.page.innerHTML;
  f.element('tide-send-form');vm.runInContext('gateFormDirty=true',f.context);
  f.render({...f.state,tab:7,stats:['10','2','0','0'],toast:'连接已刷新'});
  assert.equal(f.elements.page.innerHTML,before);assert.equal(f.elements.toast.textContent,'连接已刷新');
});
test('the six screenshot areas present their controls directly without another menu',()=>{
  const f=fixture();f.render({...f.state,tab:0});assert.doesNotMatch(f.elements.page.innerHTML,/screen-tabs|data-screen-select/);
  for(const marker of ['watch-input','最近','她身边','stats'])assert.ok(f.elements.page.innerHTML.includes(marker));
  f.render({...f.state,tab:5});assert.doesNotMatch(f.elements.page.innerHTML,/screen-tabs|data-screen-select/);
  for(const label of ['桌宠','唱歌','豆包语音','在线 Breeze','长期记忆','语音路线'])assert.ok(f.elements.page.innerHTML.includes(label));
  for(const section of ['1','2','3']){f.handlers.click({target:{closest:()=>({dataset:{maint:section}})}});assert.doesNotMatch(f.elements.page.innerHTML,/screen-tabs|data-screen-select/)}
  f.render({...f.state,tab:6,asks:Array.from({length:11},(_,i)=>({title:'求助'+i,body:'完整模板'+i}))});
  f.handlers.click({target:{closest:()=>({dataset:{settings:'2'}})}});
  f.handlers.click({target:{closest:()=>({dataset:{help:'2'}})}});assert.doesNotMatch(f.elements.page.innerHTML,/screen-tabs|data-screen-select/);
  for(let i=0;i<11;i++){assert.match(f.elements.page.innerHTML,new RegExp(`data-askcopy="${i}"`));assert.match(f.elements.page.innerHTML,new RegExp(`data-askedit="${i}"`))}
});
test('detailed capability settings open from Configure and return to all switches',()=>{
  const f=fixture();f.render({...f.state,tab:5});assert.doesNotMatch(f.elements.page.innerHTML,/id="feature-form"/);
  f.handlers.click({target:{closest:()=>({dataset:{featurePanel:'sing'}})}});assert.match(f.elements.page.innerHTML,/id="feature-form"/);assert.match(f.elements.page.innerHTML,/data-feature-back/);
  vm.runInContext('featureDirty=true',f.context);f.element('feature-feedback');
  f.handlers.click({target:{closest:()=>({dataset:{featureBack:''}})}});assert.match(f.elements.page.innerHTML,/id="feature-form"/);assert.match(f.elements['feature-feedback'].textContent,/保存或放弃/);
  vm.runInContext('featureDirty=false',f.context);f.handlers.click({target:{closest:()=>({dataset:{featureBack:''}})}});
  assert.doesNotMatch(f.elements.page.innerHTML,/id="feature-form"/);assert.match(f.elements.page.innerHTML,/capabilities-board/);
});
test('single-view content has neither a classification selector nor an always-visible pager',()=>{
  const f=fixture();f.context.entries=[['配置','<p>少量内容</p>']];
  const html=vm.runInContext('screenMenu("only",entries)',f.context);assert.doesNotMatch(html,/data-screen-select|screen-tabs/);assert.match(html,/screen-pager" aria-label="内容分页" hidden/);
});
test('a card board only paginates when it overflows and returns to a full overview after resize',()=>{
  const f=fixture(),label={},prev={},next={},pager={hidden:true,querySelectorAll:()=>[prev,next],querySelector:()=>label};let height=300;
  const tiles=Array.from({length:6},(_,i)=>({hidden:false,value:'draft '+i,getBoundingClientRect:()=>({top:Math.floor(i/2)*110,height:100})}));
  const board={children:tiles},flow={get clientHeight(){return height-(pager.hidden?0:40)},querySelector:()=>board};
  const panel={id:'responsive-board',querySelector:s=>s==='.screen-flow'?flow:pager};f.context.panel=panel;
  vm.runInContext('fitScreenPanel(panel)',f.context);assert.equal(pager.hidden,false);assert.equal(panel._screenRanges.length,2);assert.equal(tiles.filter(n=>!n.hidden).length,4);
  vm.runInContext('showScreenPage(panel,1)',f.context);assert.deepEqual(tiles.filter(n=>!n.hidden).map(n=>n.value),['draft 4','draft 5']);
  height=420;vm.runInContext('fitScreenPanel(panel)',f.context);assert.equal(pager.hidden,true);assert.equal(panel._screenRanges.length,1);assert.ok(tiles.every(n=>!n.hidden));assert.deepEqual(tiles.map(n=>n.value),Array.from({length:6},(_,i)=>'draft '+i));
});
test('board pagination keeps side-by-side cards on the same page and respects the taller card',()=>{
  const f=fixture();f.context.rects=[{top:0,height:70},{top:0,height:140},{top:150,height:60},{top:150,height:80},{top:240,height:40}];
  assert.equal(vm.runInContext('JSON.stringify(boardRanges(rects,240))',f.context),'[[0,4],[4,5]]');
});

test('tide autostart states: starting, hosted health line and missing installer',()=>{
  const f=fixture();
  const base={reachable:false,managed:false,error:'',opMsg:'',opOk:true,busy:false,state:{accounts:[],native:{},networks:[]},logs:[],messages:{}};
  f.render({...f.state,tab:7,tide:{...base,starting:true,installed:true}});
  let h=f.elements.page.innerHTML;
  assert.match(h,/正在启动潮信/);
  assert.match(h,/正在把潮信拉起来|正在启动/);
  f.render({...f.state,tab:7,tide:{...base,starting:false,installed:false}});
  h=f.elements.page.innerHTML;
  assert.match(h,/随包的潮信文件不完整或缺失/);assert.match(h,/data-setup="retry" data-setup-id="hermes"/);
  f.render({...f.state,tab:7,tide:{...base,reachable:true,managed:true,state:{accounts:[{uin:'10001',nickname:'小号',online:true}],native:{},networks:[]}}});
  h=f.elements.page.innerHTML;
  assert.match(h,/由时序之子启动 · 控制通道已握上 · 账号 1 个/);
});


test('style navigation labels and keys share that style order',()=>{
 for(const style of ['seer','array','barrier','tower','folio']){
  const f=fixture();f.render({...f.state,style});
  const labels=[...f.elements.nav.innerHTML.matchAll(/class="lb">([^<]+)/g)].map(m=>m[1]);
  const entries=JSON.parse(vm.runInContext('JSON.stringify(styleNav())',f.context));assert.deepEqual(labels,entries.map(n=>n[1]));assert.equal(new Set(entries.map(n=>n[0])).size,6);
  for(let i=1;i<=6;i++){f.handlers.keydown({key:String(i),target:{matches:()=>false},preventDefault(){}});assert.equal(f.messages.at(-1).index,entries[i-1][0]);}
 }
});
test('onboarding blocks offline, unmanaged, demo and disconnected QQ accounts',()=>{
 const f=fixture();const base={...f.state,wizardComplete:false,wizardStep:4};
 for(const tide of [{},{reachable:true,managed:false,state:{accounts:[{online:true,uin:'10001'}]}},{reachable:true,managed:true,state:{accounts:[{online:true,demo:true,uin:'10001'}]}},{reachable:true,managed:true,state:{accounts:[{online:false,uin:'10001'}]}}]){
  f.render({...base,tide});assert.match(f.elements.page.innerHTML,/data-wizard="next" disabled/);
  const count=f.messages.length;vm.runInContext('wizardCommand("next")',f.context);assert.equal(f.messages.length,count);
 }
 const tide={reachable:true,managed:true,state:{accounts:[{online:true,uin:'10001'}]}};
 f.render({...base,tide});assert.doesNotMatch(f.elements.page.innerHTML,/data-wizard="next" disabled/);
 vm.runInContext('wizardCommand("next")',f.context);assert.equal(f.messages.at(-1).cmd,'wizard');
});
test('onboarding persists through backend commands and guards keyboard navigation',()=>{
 const f=fixture();f.render({...f.state,wizardComplete:false,wizardStep:3});
 assert.match(f.elements.page.innerHTML,/页面风格/);assert.match(f.elements.nav.innerHTML,/风格主题/);
 for(const key of ['1','6','F9','F10']){const n=f.messages.length;f.handlers.keydown({key,target:{matches:()=>false},preventDefault(){}});assert.equal(f.messages.length,n);}
 f.handlers.keydown({key:'Escape',target:{matches:()=>false},preventDefault(){}});assert.equal(f.messages.at(-1).value,'exit');
 f.render({...f.state,wizardComplete:true});assert.doesNotMatch(f.elements.page.innerHTML,/wizard-shell/);
});
test('final onboarding confirmation rechecks QQ and permits no offline completion',()=>{
 const f=fixture();f.render({...f.state,wizardComplete:false,wizardStep:5,tide:{reachable:true,managed:true,state:{accounts:[]}}});
 assert.match(f.elements.page.innerHTML,/data-wizard="finish" disabled/);
 const n=f.messages.length;vm.runInContext('wizardCommand("finish")',f.context);assert.equal(f.messages.length,n);
});

test('component preparation locks eight requirements and keeps manual rows instruction-only',()=>{
 const f=fixture(),catalog=JSON.parse(fs.readFileSync(path.join(__dirname,'../../tools/components.json'),'utf8'));
 const components=catalog.components.map(c=>({...c,selected:c.tier==='required'||c.default_install}));
 const setup={state:'paused',pct:42,done:18600000,total:44200000,speed:1200000,eta:22,components,items:components.map(c=>({id:c.id,state:c.tier==='manual'?'manual':'pending',label:'检查',done:0,total:c.size_bytes}))};
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup});let h=f.elements.page.innerHTML;
 assert.match(h,/3 \/ 6/);assert.match(h,/data-wizard="next" disabled/);assert.match(h,/role="progressbar"/);assert.match(h,/可选 · 1.6 GB/);
 assert.equal((h.match(/运行必需，已锁定/g)||[]).length,8);
 assert.doesNotMatch(h,/data-setup-choice="(?:node|uv|webview2|astrbot|hermes|python|ffmpeg|bots-launchers)"/);
 assert.doesNotMatch(h,/data-setup-choice="ollama"[^>]* checked/);
 for(const id of ['memory-plugin','qq-login','bili-login','deepseek-provider']){
   const row=h.slice(h.indexOf('data-setup-row="'+id+'"'));assert.match(row,/去设置/);
   assert.doesNotMatch(row.slice(0,row.indexOf('</small>')),/MB\/s|剩余.*秒|progressbar/);
 }
 const before=f.messages.length;vm.runInContext('wizardCommand("next")',f.context);assert.equal(f.messages.length,before);
 setup.items=setup.items.map(i=>({...i,state:components.find(c=>c.id===i.id).tier==='required'?'done':i.state}));
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup});assert.doesNotMatch(f.elements.page.innerHTML,/data-wizard="next" disabled/);
 vm.runInContext('wizardCommand("next")',f.context);assert.equal(f.messages.at(-1).cmd,'wizard');
});
test('bundled tide row never offers downloads or mirrors and shares one status wording',()=>{
 const f=fixture(),catalog=JSON.parse(fs.readFileSync(path.join(__dirname,'../../tools/components.json'),'utf8'));
 const components=catalog.components.map(c=>({...c,selected:c.tier==='required'||c.default_install}));
 const items=components.map(c=>({id:c.id,state:c.tier==='manual'?'manual':'pending',label:'等待',done:0,total:c.size_bytes}));
 const setup={state:'idle',pct:0,done:0,total:0,speed:0,eta:0,components,items};
 const rowOf=h=>{const a=h.indexOf('data-setup-row="hermes"');return h.slice(a,(h.indexOf('data-setup-row="',a+10)>-1?h.indexOf('data-setup-row="',a+10):h.length))};
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup});let row=rowOf(f.elements.page.innerHTML);
 assert.match(row,/已随包/);assert.doesNotMatch(row,/换镜像|下载|MB\/s|剩余|ETA/);
 items.find(i=>i.id==='hermes').state='installing';items.find(i=>i.id==='hermes').label='初始化（本地）';
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup});row=rowOf(f.elements.page.innerHTML);
 assert.match(row,/正在初始化/);
 items.find(i=>i.id==='hermes').state='failed';items.find(i=>i.id==='hermes').label='随包文件未就绪';
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup});row=rowOf(f.elements.page.innerHTML);
 assert.match(row,/未就绪/);assert.match(row,/修复/);assert.doesNotMatch(row,/换镜像|重试/);
 items.find(i=>i.id==='hermes').state='done';items.find(i=>i.id==='hermes').label='已就绪（随安装包）';
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup});row=rowOf(f.elements.page.innerHTML);
 assert.match(row,/已就绪/);assert.doesNotMatch(row,/下载|换镜像|MB/);
});
test('component settings is a complete paginated page with repair and automatic supply controls',()=>{
 const f=fixture(),components=JSON.parse(fs.readFileSync(path.join(__dirname,'../../tools/components.json'),'utf8')).components;
 vm.runInContext('settingsSec=4',f.context);f.render({...f.state,tab:6,setup:{busy:true,components,items:[],pct:12,auto_repair:true}});
 const h=f.elements.page.innerHTML;for(const label of ['运行必需','默认安装','按需安装','稍后手动','启动时自动补装','修复'])assert.ok(h.includes(label));
 assert.match(h,/settings-components/);assert.match(h,/setup-top/);assert.match(h,/data-setup="pause"/);
 const node=h.slice(h.indexOf('data-setup-row="node"'),h.indexOf('data-setup-row="ffmpeg"'));assert.doesNotMatch(node,/卸载|skip-optional/);
});
test('welcome and workspace never start downloading; setup actions use typed IDs',()=>{
 const f=fixture();for(let wizardStep=0;wizardStep<2;++wizardStep)f.render({...f.state,wizardComplete:false,wizardStep,setup:{state:'idle'}});
 assert.equal(f.messages.filter(m=>m.cmd==='setup:start').length,0);
 f.handlers.click({target:{closest:s=>s.includes('data-setup]')?{dataset:{setup:'retry',setupId:'node'}}:null}});
 assert.equal(f.messages.at(-1).cmd,'setup:retry');assert.equal(f.messages.at(-1).id,'node');
});

test('diagnostics excludes unrequested optional downloads and retains requested failures',()=>{
 const f=fixture(),components=JSON.parse(fs.readFileSync(path.join(__dirname,'../../tools/components.json'),'utf8')).components.map(c=>({...c,selected:c.default_install}));
 f.render({...f.state,tab:4,setup:{components,items:components.map(c=>({id:c.id,state:c.id==='edge-tts'?'failed':c.tier==='required'?'done':'pending'}))}});
 const h=f.elements.page.innerHTML;assert.match(h,/data-setup-open="rift"/);assert.match(h,/data-setup-open="edge-tts"/);
 for(const id of ['ollama','snowluma','embedding-model','soulx-singer','dsh-pet','memory-plugin'])assert.ok(!h.includes('data-setup-open="'+id+'"'));
});

test('manual setup entries reach actual configuration pages and preserve the wizard gate',()=>{
 const f=fixture(),components=JSON.parse(fs.readFileSync(path.join(__dirname,'../../tools/components.json'),'utf8')).components;
 const setup={components,items:components.map(c=>({id:c.id,state:c.tier==='required'?'done':'manual'})),state:'done'};
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup,wizardManual:'memory-plugin'});
 assert.match(f.elements.page.innerHTML,/data-action="astrbot:open"/);assert.match(f.elements.page.innerHTML,/data-setup="settings-back"/);assert.doesNotMatch(f.elements.page.innerHTML,/data-wizard="next"/);
 f.handlers.keydown({key:'Escape',target:{matches:()=>false},preventDefault(){}});assert.equal(f.messages.at(-1).cmd,'setup:settings-back');
 f.render({...f.state,wizardComplete:false,wizardStep:2,setup,wizardManual:'bili-login'});assert.match(f.elements.page.innerHTML,/B 站配置/);assert.match(f.elements.page.innerHTML,/id="config-form"/);
 f.render({...f.state,setup});
 const click=id=>f.handlers.click({target:{closest:s=>s.includes('data-setup]')?{dataset:{setupSettings:id}}:null}});
 click('qq-login');assert.equal(f.messages.at(-1).index,7);
 click('bili-login');assert.equal(f.messages.at(-2).index,1);assert.equal(f.messages.at(-1).cmd,'section');assert.equal(f.messages.at(-1).index,3);
});

test('installed optional component repair requests forced retry rather than a satisfied install check',()=>{
 const f=fixture();
 f.render({...f.state,setup:{components:[{id:'rift',name:'Rift',tier:'default',selected:true}],items:[{id:'rift',state:'done',label:'已在位'}]}});
 f.context.component={id:'rift',name:'Rift',tier:'default',selected:true};
 const html=vm.runInContext('setupRow(component,false)',f.context);
 assert.match(html,/data-setup="retry" data-setup-id="rift">修复/);
 assert.match(html,/data-setup="uninstall" data-setup-id="rift">卸载/);
});

test('legacy theme aliases map to canonical names and never appear as preset labels',()=>{
 const f=fixture();for(const [old,now] of [['dark','nightfall'],['light','daylight'],['contrast','boundary'],['moon','moon'],['suit','suit']]){
  f.render({...f.state,theme:old,tab:6});assert.equal(f.context.document.documentElement.dataset.theme,now);
 }
 const h=f.elements.page.innerHTML;for(const name of ['长夜','白昼','断界','月轨','特行'])assert.ok(h.includes(name));
});
test('update channel and rollback controls route explicit commands',()=>{
 const f=fixture();f.render({...f.state,tab:6});f.handlers.click({target:{closest:()=>({dataset:{updateChannel:'beta'}})}});assert.equal(f.messages.at(-1).cmd,'update-channel');assert.equal(f.messages.at(-1).value,'beta');
 f.handlers.click({target:{closest:()=>({dataset:{updateRollback:''}})}});assert.equal(f.messages.at(-1).cmd,'update-rollback');
});
