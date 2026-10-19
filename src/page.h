#pragma once
// Dashboard HTML for the C3 AdBlocker web UI, kept in its own header so the
// Arduino IDE preprocessor doesn't choke on the inlined markup (issue #6).
// The UI is bilingual (中文 / English); the chosen language is saved in
// localStorage and toggled with the header button.

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>C3 AdBlock</title><style>
body{font:14px system-ui,sans-serif;margin:0;background:#0d1117;color:#c9d1d9}
header{background:#161b22;padding:14px 18px;border-bottom:1px solid #30363d;display:flex;align-items:center;justify-content:space-between}
h1{margin:0;font-size:18px}h1 span{color:#3fb950}.wrap{padding:16px;max-width:1000px;margin:auto}
.cards{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:16px}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:12px 16px;flex:1;min-width:120px}
.card .v{font-size:22px;font-weight:600}.card .l{color:#8b949e;font-size:12px}
table{width:100%;border-collapse:collapse;background:#161b22;border-radius:8px;overflow:hidden;margin-bottom:18px}
th,td{padding:8px 10px;text-align:left;border-bottom:1px solid #21262d;font-size:13px}
th{background:#21262d;color:#8b949e}tr:hover td{background:#1c2128}
.b{color:#f85149}.a{color:#3fb950}.tag{background:#30363d;border-radius:4px;padding:1px 6px;font-size:11px}
button{background:#21262d;color:#c9d1d9;border:1px solid #30363d;border-radius:5px;padding:4px 9px;cursor:pointer}
button:hover{background:#30363d}.ban{color:#f85149}input{background:#0d1117;border:1px solid #30363d;color:#c9d1d9;border-radius:5px;padding:6px}
h2{font-size:14px;color:#8b949e;margin:18px 0 8px}
</style></head><body>
<header><h1>🛡️ C3 AdBlock <span id=host></span></h1><button id=langbtn onclick=toggleLang()>中文</button></header><div class=wrap>
<div id=credwarn data-i18n=credwarn style="display:none;background:#3b1d1d;border:1px solid #f85149;color:#ffb3ae;border-radius:8px;padding:10px 14px;margin-bottom:14px;font-size:13px">
⚠️ <b>Default credentials in use.</b> WEB_PASS/OTA_PASS in <code>secrets.h</code> are still the placeholder values — anyone can read them in the public repo. Set real values and reflash.
</div>
<div id=blockbar style="display:flex;align-items:center;gap:12px;margin-bottom:14px;padding:12px 14px;background:#161b22;border:1px solid #30363d;border-radius:8px">
<span id=blockdot style=font-size:20px>🛡️</span><b id=blockstate style=flex:1 data-on=1>Blocking active</b>
<select id=pausedur style="background:#0d1117;border:1px solid #30363d;color:#c9d1d9;border-radius:5px;padding:5px"><option value=30 data-i18n=dur30s>30s</option><option value=300 selected data-i18n=dur5m>5 min</option><option value=1800 data-i18n=dur30m>30 min</option><option value=0 data-i18n=durInf>until I re-enable</option></select>
<button id=pausebtn onclick=togglePause() data-i18n=pause>Pause</button></div>
<div class=cards id=sys></div>
<h2 data-i18n=hClients>CLIENTS</h2><table id=ct><thead><tr><th data-i18n=thClient>Client</th><th data-i18n=thMac>MAC</th><th data-i18n=thBlocked>Blocked</th><th data-i18n=thAllowed>Allowed</th><th></th></tr></thead><tbody></tbody></table>
<h2 data-i18n=hCustom>CUSTOM BLOCKED DOMAINS</h2>
<div style=margin-bottom:8px><input id=dom data-i18n-ph=phDom placeholder="ads.example.com" size=30><button onclick=addDom() data-i18n=btnBlockDomain>Block domain</button></div>
<table id=cl><tbody></tbody></table>
<h2 data-i18n=hUpload>BLOCKLIST &mdash; UPLOAD</h2>
<form id=upf style=margin-bottom:6px><input type=file id=blf accept=.bin><button data-i18n=btnUpload>Upload blocklist</button> <span id=upmsg style=color:#8b949e></span></form>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px" data-i18n=hintUpload>build <code>blocklist.bin</code> with <code>tools/build_blocklist.py</code>, then upload here &mdash; no USB</div>
<h2 data-i18n=hRemote>BLOCKLIST &mdash; REMOTE AUTO-UPDATE</h2>
<div style=margin-bottom:6px><input id=uurl data-i18n-ph=phUrl placeholder="https://host/blocklist.bin" size=40> every <input id=uiv size=2 value=24>h
<button onclick=saveUpd() data-i18n=btnSave>Save</button> <button onclick=fetchNow() data-i18n=btnFetchNow>Fetch now</button></div>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px"><span data-i18n=hintRemote>device pulls a prebuilt <code>blocklist.bin</code> on a schedule (e.g. a GitHub release asset). last:</span> <span id=ustat>&mdash;</span></div>
<h2 data-i18n=hFw>FIRMWARE &mdash; OTA UPDATE</h2>
<form id=fwf style=margin-bottom:6px><input type=file id=fwb accept=.bin><button data-i18n=btnFlash>Flash firmware</button> <span id=fwmsg style=color:#8b949e></span></form>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px" data-i18n=hintFw>upload <code>.pio/build/c3/firmware.bin</code> &mdash; device verifies it and reboots into it</div>
<h2 data-i18n=hWifi>WIFI</h2>
<div style=margin-bottom:18px><button onclick="if(confirm(t('confirmForget')))forgetWifi()" data-i18n=btnForget>Forget WiFi</button></div>
</div><script>
var LANG=localStorage.getItem('c3lang')||'en';
var I18N={
credwarn:{en:'⚠️ <b>Default credentials in use.</b> WEB_PASS/OTA_PASS in <code>secrets.h</code> are still the placeholder values — anyone can read them in the public repo. Set real values and reflash.',zh:'⚠️ <b>正在使用默认凭据。</b> <code>secrets.h</code> 里的 WEB_PASS/OTA_PASS 仍是占位值——任何人都能在公开仓库里读到。请设置真实值并重新刷写。'},
blockOn:{en:'Blocking active',zh:'拦截已开启'},paused:{en:'Paused',zh:'已暂停'},
pausedIn:{en:'Paused — resumes in {n}s',zh:'已暂停 — {n} 秒后恢复'},
pause:{en:'Pause',zh:'暂停'},resume:{en:'Resume',zh:'恢复'},
dur30s:{en:'30s',zh:'30 秒'},dur5m:{en:'5 min',zh:'5 分钟'},dur30m:{en:'30 min',zh:'30 分钟'},durInf:{en:'until I re-enable',zh:'直到我重新启用'},
hClients:{en:'CLIENTS',zh:'客户端'},thClient:{en:'Client',zh:'客户端'},thMac:{en:'MAC',zh:'MAC'},thBlocked:{en:'Blocked',zh:'拦截'},thAllowed:{en:'Allowed',zh:'放行'},
hCustom:{en:'CUSTOM BLOCKED DOMAINS',zh:'自定义拦截域名'},phDom:{en:'ads.example.com',zh:'ads.example.com'},btnBlockDomain:{en:'Block domain',zh:'拦截域名'},
hUpload:{en:'BLOCKLIST — UPLOAD',zh:'拦截列表 — 上传'},btnUpload:{en:'Upload blocklist',zh:'上传拦截列表'},
hintUpload:{en:'build <code>blocklist.bin</code> with <code>tools/build_china_blocklist.py</code>, then upload here — no USB',zh:'用 <code>tools/build_china_blocklist.py</code> 构建 <code>blocklist.bin</code>，然后在此上传 — 无需 USB'},
hRemote:{en:'BLOCKLIST — REMOTE AUTO-UPDATE',zh:'拦截列表 — 远程自动更新'},phUrl:{en:'https://host/blocklist.bin',zh:'https://host/blocklist.bin'},btnSave:{en:'Save',zh:'保存'},btnFetchNow:{en:'Fetch now',zh:'立即拉取'},
hintRemote:{en:'device pulls a prebuilt <code>blocklist.bin</code> on a schedule (e.g. a GitHub release asset). last:',zh:'设备按计划拉取预构建的 <code>blocklist.bin</code>（例如 GitHub release 附件）。上次：'},
hFw:{en:'FIRMWARE — OTA UPDATE',zh:'固件 — OTA 更新'},btnFlash:{en:'Flash firmware',zh:'刷写固件'},
hintFw:{en:'upload <code>.pio/build/c3/firmware.bin</code> — device verifies it and reboots into it',zh:'上传 <code>.pio/build/c3/firmware.bin</code> — 设备校验后重启进入新固件'},
hWifi:{en:'WIFI',zh:'无线网络'},btnForget:{en:'Forget WiFi',zh:'忘记 WiFi'},confirmForget:{en:'Forget saved WiFi and reboot into the setup portal?',zh:'忘记已保存的 WiFi 并重启进入配网门户？'},
totalBlocked:{en:'Total blocked',zh:'总拦截'},totalAllowed:{en:'Total allowed',zh:'总放行'},blocklistLabel:{en:'Blocklist',zh:'拦截列表'},domainsSuffix:{en:' domains',zh:' 个域名'},clientsLabel:{en:'Clients',zh:'客户端数'},wifiLabel:{en:'WiFi',zh:'WiFi'},tempLabel:{en:'Temp',zh:'温度'},freeRam:{en:'Free RAM',zh:'空闲内存'},uptime:{en:'Uptime',zh:'运行时间'},
bannedTag:{en:'BANNED',zh:'已拉黑'},ban:{en:'Ban',zh:'拉黑'},unban:{en:'Unban',zh:'解除拉黑'},noneYet:{en:'none yet',zh:'暂无'},remove:{en:'remove',zh:'移除'},
fetching:{en:'fetching...',zh:'拉取中...'},flashing:{en:'flashing {n} MB...',zh:'刷写中 {n} MB...'},rebooting:{en:'✓ rebooting, reconnect in ~15s',zh:'✓ 重启中，约 15 秒后重连'},fwFail:{en:'✗ ',zh:'✗ '},uploading:{en:'uploading {n} MB...',zh:'上传中 {n} MB...'},updated:{en:'✓ updated',zh:'✓ 已更新'},uploadFailed:{en:'✗ upload failed',zh:'✗ 上传失败'}
};
function t(k,s){var o=(I18N[k]||{})[LANG];if(o===undefined)o=(I18N[k]&&I18N[k].en)||k;if(s)for(var x in s)o=o.split('{'+x+'}').join(s[x]);return o;}
function translate(){langbtn.textContent=(LANG==='zh')?'EN':'中文';
document.querySelectorAll('[data-i18n]').forEach(function(el){var v=t(el.getAttribute('data-i18n'));if(el.tagName==='OPTION')el.textContent=v;else el.innerHTML=v;});
document.querySelectorAll('[data-i18n-ph]').forEach(function(el){el.placeholder=t(el.getAttribute('data-i18n-ph'));});}
function toggleLang(){LANG=(LANG==='zh')?'en':'zh';localStorage.setItem('c3lang',LANG);translate();load();}
function fmt(n){return n.toLocaleString()}
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
// A plain <img>/<form> CSRF can't set a custom header, only same-origin fetch()
// can — so requiring this on every mutating request blocks drive-by CSRF even
// though the server can't otherwise tell a forged request from a real one over
// plain HTTP Basic Auth (browsers auto-replay cached Basic Auth cross-origin).
const CSRF_HDRS={'X-Requested-With':'c3-adblock'}
function togglePause(){fetch(blockstate.dataset.on=='1'?'/pause?s='+pausedur.value:'/resume',{headers:CSRF_HDRS}).then(load);}
async function load(){let s=await(await fetch('/stats.json')).json();
host.textContent='@ '+s.ip;
credwarn.style.display=s.defcreds?'block':'none';
let on=s.blocking!==false;blockstate.dataset.on=on?'1':'0';
blockdot.textContent=on?'🛡️':'⏸️';blockbar.style.borderColor=on?'#30363d':'#f0883e';
blockstate.textContent=on?t('blockOn'):(s.resumeIn>0?t('pausedIn',{n:s.resumeIn}):t('paused'));
pausebtn.textContent=on?t('pause'):t('resume');pausedur.style.display=on?'':'none';
sys.innerHTML=[[t('totalBlocked'),fmt(s.blocked),'b'],[t('totalAllowed'),fmt(s.allowed),'a'],[t('blocklistLabel'),fmt(s.domains)+t('domainsSuffix'),''],[t('clientsLabel'),s.clients.length,''],[t('wifiLabel'),s.rssi+' dBm',''],[t('tempLabel'),s.temp+' °C',''],[t('freeRam'),Math.round(s.heap/1024)+' KB',''],[t('uptime'),s.uptime,'']]
.map(c=>`<div class=card><div class="v ${c[2]}">${c[1]}</div><div class=l>${c[0]}</div></div>`).join('');
ct.tBodies[0].innerHTML=s.clients.sort((a,b)=>(b.blocked+b.allowed)-(a.blocked+a.allowed)).map(c=>
`<tr><td>${c.ip}${c.banned?' <span class=tag style=color:#f85149>'+t('bannedTag')+'</span>':''}</td><td>${c.mac}</td>
<td class=b>${fmt(c.blocked)}</td><td class=a>${fmt(c.allowed)}</td>
<td><button class=ban data-ip="${c.ip}">${c.banned?t('unban'):t('ban')}</button></td></tr>`).join('');
cl.tBodies[0].innerHTML=s.custom.map(d=>`<tr><td>${esc(d)}</td><td style=text-align:right><button class=rmbtn data-d="${esc(d)}">${t('remove')}</button></td></tr>`).join('')||`<tr><td style=color:#8b949e>${t('noneYet')}</td></tr>`;
if(document.activeElement!=uurl)uurl.value=s.upurl||'';
if(document.activeElement!=uiv)uiv.value=s.upiv||24;
ustat.textContent=s.upstat||'—';}
function addDom(){let d=dom.value.trim();if(d){fetch('/addblock?d='+encodeURIComponent(d),{headers:CSRF_HDRS}).then(()=>{dom.value='';load()})}}
ct.addEventListener('click',e=>{if(e.target.classList.contains('ban'))fetch('/ban?ip='+e.target.dataset.ip,{headers:CSRF_HDRS}).then(load)});
cl.addEventListener('click',e=>{if(e.target.classList.contains('rmbtn'))fetch('/unblock?d='+encodeURIComponent(e.target.dataset.d),{headers:CSRF_HDRS}).then(load)});
function saveUpd(){fetch('/setupdate?u='+encodeURIComponent(uurl.value.trim())+'&h='+(parseInt(uiv.value)||24),{headers:CSRF_HDRS}).then(load)}
function fetchNow(){ustat.textContent=t('fetching');fetch('/fetchnow',{headers:CSRF_HDRS}).then(r=>r.text()).then(x=>{ustat.textContent=x;load()})}
function forgetWifi(){fetch('/forgetwifi',{headers:CSRF_HDRS}).then(r=>r.text()).then(x=>alert(x))}
fwf.onsubmit=async e=>{e.preventDefault();let f=fwb.files[0];if(!f)return;fwmsg.textContent=t('flashing',{n:(f.size/1048576).toFixed(2)});
let fd=new FormData();fd.append('f',f);
try{let r=await fetch('/update',{method:'POST',headers:CSRF_HDRS,body:fd});fwmsg.textContent=r.ok?t('rebooting'):t('fwFail')+await r.text();}
catch(_){fwmsg.textContent=t('rebooting');}};
upf.onsubmit=async e=>{e.preventDefault();let f=blf.files[0];if(!f)return;
upmsg.textContent=t('uploading',{n:(f.size/1048576).toFixed(2)});
let fd=new FormData();fd.append('f',f);
try{let r=await fetch('/upload',{method:'POST',headers:CSRF_HDRS,body:fd});upmsg.textContent=r.ok?t('updated'):t('fwFail')+await r.text();}
catch(_){upmsg.textContent=t('uploadFailed');}
blf.value='';setTimeout(load,600);};
translate();load();setInterval(load,3000);
</script></body></html>)HTML";
