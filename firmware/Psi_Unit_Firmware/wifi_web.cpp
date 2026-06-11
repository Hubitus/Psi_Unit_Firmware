/*
 * wifi_web.cpp — web pages and REST API for browser-based configuration
 *
 * HTML/CSS/JS dashboard: sensor readings, output toggles, all settings.
 * Reads via app_buildSnapshot() and settings_access; writes via app_dispatchApply().
 */

#include "wifi_web.h"

#if WIFI_ENABLE && defined(ARDUINO_ARCH_ESP32) && !defined(SIM_BUILD)

#include "app_snapshot.h"
#include "app_commands.h"
#include "app_effects.h"
#include "system_info.h"
#include "config.h"
#include "settings_field.h"
#include "settings_access.h"
#include "wifi_manager.h"
#include "wifi_qr.h"
#include "sensor_log.h"
#include "web_logo.h"
#include "wifi_json_buf.h"
#include <ESPAsyncWebServer.h>
#include <esp_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char WIFI_WEB_CSS[] PROGMEM = R"rawliteral(
body{font-family:system-ui,sans-serif;margin:12px;background:#0f1419;color:#e7ecf3;padding-top:4px}
body.has-banner{padding-top:44px}
.page-head{margin:0 0 6px}
.page-head .logo{display:block;max-width:min(100%,360px);height:auto}
h1{font-size:1.2rem;margin:0 0 6px}
.meta{font-size:.85rem;color:#8b949e;margin-bottom:10px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:10px 0}
.card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:10px}
.card h2{font-size:.75rem;color:#8b949e;margin:0 0 4px;font-weight:600;text-transform:uppercase}
.val{font-size:1.35rem;font-weight:600}
.val.err{color:#f85149}
.out{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px}
.chip{display:block;background:#161b22;border:1px solid #30363d;border-radius:6px;padding:8px 10px;font-size:.85rem;text-decoration:none;color:inherit;cursor:pointer}
.chip.on{border-color:#238636;background:#122117}
.chip .m{color:#8b949e;font-size:.75rem;margin-top:2px}
.chip-hint{font-size:.75rem;color:#58a6ff;margin:4px 0 0}
.dash-charts{margin-bottom:12px}
.dash-settings{margin-top:12px;padding:14px 12px 12px;background:#1c2128;border:1px solid #30363d;border-radius:8px}
.dash-settings .out{margin-top:0}
.dash-settings .btn-ovr{margin-top:10px}
.dash-settings .navfoot{margin-top:12px}
.chart-panel{margin:10px 0 12px;padding:12px;background:#1c2128;border:1px solid #30363d;border-radius:8px}
.chart-panel .range-row{margin:0 0 8px}
.chart-panel .chart-meta{margin-top:0}
.chart-panel .chart-wrap{margin:8px 0 0;background:#161b22}
.conn-lost{display:none;position:fixed;top:0;left:0;right:0;z-index:1000;background:#da3633;color:#fff;text-align:center;padding:10px 12px;font-size:.9rem;font-weight:600}
.sec{font-weight:600;color:#58a6ff;margin:14px 0 6px;font-size:.9rem}
.row{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid #21262d;font-size:.9rem}
.lbl{color:#8b949e;margin-right:8px}
.rval{text-align:right;word-break:break-all}
.fld{margin:10px 0}
.fld label{font-size:.78rem;color:#8b949e;display:block;margin-bottom:3px}
.fld select,.fld input{width:100%;padding:8px;background:#0d1117;color:#e7ecf3;border:1px solid #30363d;border-radius:4px;box-sizing:border-box;font-size:.95rem}
.btnrow{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin-top:16px}
.btn{padding:10px 8px;border:0;border-radius:6px;font-size:.9rem;text-align:center;cursor:pointer;text-decoration:none;display:flex;align-items:center;justify-content:center}
.btn-save{background:#238636;color:#fff}
.btn-cancel{background:#30363d;color:#e7ecf3}
.btn-home{background:#21262d;color:#58a6ff;border:1px solid #30363d}
.btn-extra{margin-top:10px;padding:10px;background:#1f6feb;color:#fff;border:0;border-radius:6px;width:100%;font-size:.9rem}
.btn-dng{margin-top:10px;padding:10px;background:#b62324;color:#fff;border:0;border-radius:6px;width:100%;font-size:.9rem}
.msg{font-size:.8rem;color:#8b949e;margin-top:10px;min-height:1.2em}
.navfoot{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:16px}
.btn-nav{display:block;padding:12px;background:#21262d;color:#e7ecf3;border:1px solid #30363d;border-radius:6px;text-decoration:none;text-align:center;font-size:.95rem;cursor:pointer}
.btn-nav:active{opacity:.85}
.btn-ovr{display:block;width:100%;margin-top:12px;padding:14px;background:#1f6feb;color:#fff;border:0;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer}
.btn-ovr.on{background:#238636}
.st-live{font-size:.85rem;color:#8b949e;margin-bottom:12px;padding:8px;background:#161b22;border:1px solid #21262d;border-radius:6px}
.sect{font-size:.95rem;font-weight:600;color:#58a6ff;margin:18px 0 8px;padding-top:10px;border-top:1px solid #21262d}
.sect:first-child{border-top:0;margin-top:0;padding-top:0}
.sect-hint{font-size:.78rem;color:#6e7681;margin:-2px 0 10px;line-height:1.4}
.fld-hint{font-size:.72rem;color:#6e7681;margin-top:3px;line-height:1.35}
.hub-link{display:block;padding:14px;margin:8px 0;background:#161b22;border:1px solid #30363d;border-radius:8px;text-decoration:none;color:inherit}
.hub-link b{display:block;color:#e7ecf3;font-size:.95rem;margin-bottom:3px}
.hub-link span{font-size:.78rem;color:#8b949e}
.wifi-box{margin:10px 0;padding:12px;background:#161b22;border:1px solid #21262d;border-radius:8px}
.wifi-row{margin:8px 0}
.wifi-row b{display:block;font-size:.78rem;color:#8b949e;margin-bottom:3px}
.wifi-val{font-size:.95rem;word-break:break-all}
.wifi-qr{margin:12px auto;text-align:center}
.wifi-qr svg,.wifi-qr img{max-width:100%;height:auto;border-radius:8px;background:#fff;padding:8px;box-sizing:border-box}
.clk-block{margin:10px 0;padding:12px;background:#161b22;border:1px solid #21262d;border-radius:8px}
.clk-lbl{font-size:.78rem;color:#8b949e;margin-bottom:4px}
.clk-val{font-size:1.15rem;font-weight:600;font-variant-numeric:tabular-nums;text-align:center}
.sync-ok{display:none;background:#238636;color:#fff;padding:16px;border-radius:8px;text-align:center;font-size:1rem;font-weight:700;margin:14px 0;box-shadow:0 0 0 2px #2ea043}
.sync-ok.show{display:block;animation:pulseOk .45s ease}
@keyframes pulseOk{from{transform:scale(.96);opacity:.7}to{transform:scale(1);opacity:1}}
.card-link{text-decoration:none;color:inherit;display:block;cursor:pointer}
.card-link:active{opacity:.85}
.chart-wrap{margin:12px 0;background:#161b22;border:1px solid #21262d;border-radius:8px;padding:8px;overflow:hidden}
.chart-wrap svg{width:100%;height:auto;display:block;max-height:min(42vh,360px);min-height:130px;touch-action:manipulation}
.range-row{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0}
.range-btn{padding:8px 12px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#e7ecf3;font-size:.85rem;cursor:pointer}
.range-btn.on{border-color:#238636;background:#122117}
.opt-row{display:flex;flex-wrap:wrap;gap:6px;margin:4px 0 0}
.opt-btn{padding:10px 12px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#e7ecf3;font-size:.9rem;font-weight:600;cursor:pointer;flex:1 1 auto;min-width:3.2em;text-align:center}
.opt-btn.on{border-color:#238636;background:#122117;color:#e7ecf3}
.opt-btn:active{opacity:.85}
.toggle-btn{width:100%;margin-top:4px;padding:12px 14px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#e7ecf3;font-size:.95rem;cursor:pointer;text-align:left}
.toggle-btn.on{border-color:#238636;background:#122117}
.toggle-btn:active{opacity:.85}
.wifi-tmo-row .opt-btn{min-width:3.8em;font-size:.82rem;padding:8px 10px}
body.dev-page h1{font-size:1.05rem;margin:0 0 4px}
body.dev-page .st-live{margin-bottom:6px;padding:6px 8px;font-size:.8rem}
body.dev-page .sect{margin:10px 0 4px;padding-top:6px;font-size:.85rem}
body.dev-page .sect-hint{margin-bottom:4px;font-size:.72rem}
body.dev-page .fld-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px 8px;margin:2px 0 8px;align-items:stretch}
body.dev-page .fld-grid>.fld{margin:0;display:flex;flex-direction:column}
body.dev-page .fld-grid>.fld-full{grid-column:1/-1}
body.dev-page .fld-grid .fld label{font-size:.7rem;margin-bottom:2px;line-height:1.2}
body.dev-page .fld-grid .fld-hint{display:none}
body.dev-page .val-btn{width:100%;padding:9px 6px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#e7ecf3;font-size:1.05rem;font-weight:600;cursor:pointer;font-variant-numeric:tabular-nums}
body.dev-page .val-btn:active{opacity:.85}
body.dev-page .fld-grid .val-btn,
body.dev-page .fld-grid .opt-row{flex:1 1 auto;margin:0;min-height:40px}
body.dev-page .fld-grid .val-btn{padding:8px 6px;font-size:.92rem;font-weight:600;display:flex;align-items:center;justify-content:center;line-height:1.2;box-sizing:border-box}
body.dev-page .fld-grid .opt-row{display:flex;flex-wrap:nowrap;align-items:stretch;gap:6px}
body.dev-page .fld-grid .opt-btn{flex:1 1 0;min-width:0;min-height:40px;padding:8px 4px;font-size:.88rem;display:flex;align-items:center;justify-content:center;line-height:1.2;box-sizing:border-box}
body.dev-page .toggle-btn{margin-top:2px;padding:10px 12px;font-size:.9rem}
body.dev-page .opt-row{margin:2px 0 0}
body.dev-page .opt-btn{padding:8px 10px;font-size:.82rem;min-width:2.8em}
.wheel-modal{position:fixed;inset:0;z-index:2000;background:rgba(0,0,0,.55);display:none;align-items:flex-end;justify-content:center}
.wheel-modal.show{display:flex}
.wheel-sheet{width:100%;max-width:420px;background:#161b22;border:1px solid #30363d;border-radius:12px 12px 0 0;padding:12px 12px calc(12px + env(safe-area-inset-bottom))}
.wheel-sheet-title{font-size:.95rem;font-weight:600;color:#e7ecf3;margin-bottom:8px;text-align:center}
.wheel-wrap{position:relative;height:176px;overflow:hidden;background:#0d1117;border:1px solid #30363d;border-radius:8px}
.wheel-wrap::before,.wheel-wrap::after{content:'';position:absolute;left:0;right:0;height:44px;z-index:2;pointer-events:none}
.wheel-wrap::before{top:0;background:linear-gradient(to bottom,#0d1117 30%,transparent)}
.wheel-wrap::after{bottom:0;background:linear-gradient(to top,#0d1117 30%,transparent)}
.wheel-band{position:absolute;left:8px;right:8px;top:50%;height:44px;margin-top:-22px;border:1px solid #58a6ff;border-radius:6px;background:rgba(22,27,34,.45);pointer-events:none;z-index:1}
.wheel-col{height:100%;overflow-y:auto;scroll-snap-type:y mandatory;-webkit-overflow-scrolling:touch;scrollbar-width:none;overscroll-behavior:contain;touch-action:pan-y}
.wheel-col::-webkit-scrollbar{display:none}
.wheel-pad{height:66px;flex-shrink:0}
.wheel-item{height:44px;display:flex;align-items:center;justify-content:center;scroll-snap-align:center;font-size:1.2rem;font-weight:500;color:#6e7681;font-variant-numeric:tabular-nums}
.wheel-item.sel{color:#fff;font-weight:700}
.wheel-sheet-ft{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.wheel-sheet-ft .btn{margin:0}
.chart-meta{font-size:.8rem;color:#8b949e;margin:6px 0 10px}
.chart-empty{font-size:.9rem;color:#8b949e;text-align:center;padding:40px 12px}
)rawliteral";

static const char WIFI_CONN_JS[] PROGMEM = R"rawliteral(
function setConn(ok){
  const b=document.getElementById('connBanner');
  if(!b)return;
  b.style.display=ok?'none':'block';
  document.body.classList.toggle('has-banner',!ok);
}
async function fetchJson(url){
  const r=await fetch(url,{cache:'no-store'});
  if(!r.ok)throw new Error(r.status);
  setConn(true);
  return r.json();
}
async function psiPostUrl(url,params){
  const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams(params)});
  if(!r.ok)throw new Error(r.status);
  setConn(true);
  return r;
}
async function postForm(params){
  const r=await psiPostUrl('/api/command',params);
  return r.json();
}
async function checkConn(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok)throw new Error(r.status);
    setConn(true);
  }catch(e){setConn(false);}
}
)rawliteral";

#if WIFI_WEB_AUTH_ENABLE
static const char WIFI_CONN_JS_AUTH[] PROGMEM = R"rawliteral(
const PSI_SESS_KEY='psiSess';
function psiGetToken(){return sessionStorage.getItem(PSI_SESS_KEY)||'';}
function psiSetToken(t){if(t)sessionStorage.setItem(PSI_SESS_KEY,t);else sessionStorage.removeItem(PSI_SESS_KEY);}
function psiAuthHeaders(extra){
  const h=Object.assign({},extra||{});
  const t=psiGetToken();
  if(t)h['X-Psi-Session']=t;
  return h;
}
async function psiLogin(password){
  const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams({password})});
  if(!r.ok)throw new Error('auth');
  const j=await r.json();
  if(!j.ok||!j.token)throw new Error('auth');
  psiSetToken(j.token);
  return j.token;
}
async function psiEnsureAuth(){
  if(psiGetToken())return psiGetToken();
  const p=prompt('Wi-Fi Soft Access Point password (OLED: SET → WiFi → QR):');
  if(!p)throw new Error('auth');
  return psiLogin(p);
}
async function psiFetch(url,opts){
  await psiEnsureAuth();
  const o=Object.assign({cache:'no-store'},opts||{});
  o.headers=psiAuthHeaders(o.headers);
  let r=await fetch(url,o);
  if(r.status===401){
    psiSetToken('');
    await psiEnsureAuth();
    o.headers=psiAuthHeaders(o.headers);
    r=await fetch(url,o);
  }
  return r;
}
function setConn(ok){
  const b=document.getElementById('connBanner');
  if(!b)return;
  b.style.display=ok?'none':'block';
  document.body.classList.toggle('has-banner',!ok);
}
async function fetchJson(url){
  const r=await psiFetch(url);
  if(!r.ok)throw new Error(r.status);
  setConn(true);
  return r.json();
}
async function psiPostUrl(url,params){
  const r=await psiFetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams(params)});
  if(!r.ok)throw new Error(r.status);
  setConn(true);
  return r;
}
async function postForm(params){
  const r=await psiPostUrl('/api/command',params);
  return r.json();
}
async function checkConn(){
  try{
    const r=await psiFetch('/api/status');
    if(!r.ok)throw new Error(r.status);
    setConn(true);
  }catch(e){setConn(false);}
}
)rawliteral";
#endif

static const char *wifi_connJs() {
#if WIFI_WEB_AUTH_ENABLE
  return WIFI_CONN_JS_AUTH;
#else
  return WIFI_CONN_JS;
#endif
}

static const char WIFI_PAGE_DASH_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Psi Unit</title>
<link rel="icon" href="/logo.svg" type="image/svg+xml">
<style>
)rawliteral";

#if SENSOR_LOG_ENABLE
static const char WIFI_PAGE_DASH_HTML2[] PROGMEM = R"rawliteral(
</style>
</head>
<body>
<div id="connBanner" class="conn-lost">No connection to device</div>
<div class="page-head"><img src="/logo.svg" alt="Psi Unit" class="logo" width="198" height="62"></div>
<div class="meta" id="meta">Loading...</div>
<div class="dash-charts">
<div class="grid">
  <a href="/hist/inc" class="card card-link"><h2>INC °C</h2><div class="val" id="inc">--</div></a>
  <a href="/hist/grh_t" class="card card-link"><h2>GRH °C</h2><div class="val" id="grhT">--</div></a>
  <a href="/hist/grh_h" class="card card-link"><h2>GRH %</h2><div class="val" id="grhH">--</div></a>
  <div class="card"><h2>Time</h2><div class="val" id="clk" style="font-size:1rem">--</div></div>
</div>
<div class="chip-hint">Tap a sensor reading for history</div>
</div>
<div class="dash-settings">
<div class="chip-hint" style="margin-top:0">Tap device status for settings</div>
<div class="out" id="outs"></div>
<button type="button" class="btn-ovr" id="btnOvr">Light override (OVR)</button>
<div class="navfoot">
  <a href="/dev/settings" class="btn-nav">Settings</a>
  <a href="/sys" class="btn-nav">System Info</a>
</div>
</div>
<script>
)rawliteral";
#else
static const char WIFI_PAGE_DASH_HTML2[] PROGMEM = R"rawliteral(
</style>
</head>
<body>
<div id="connBanner" class="conn-lost">No connection to device</div>
<div class="page-head"><img src="/logo.svg" alt="Psi Unit" class="logo" width="198" height="62"></div>
<div class="meta" id="meta">Loading...</div>
<div class="dash-charts">
<div class="grid">
  <div class="card"><h2>INC °C</h2><div class="val" id="inc">--</div></div>
  <div class="card"><h2>GRH °C</h2><div class="val" id="grhT">--</div></div>
  <div class="card"><h2>GRH %</h2><div class="val" id="grhH">--</div></div>
  <div class="card"><h2>Time</h2><div class="val" id="clk" style="font-size:1rem">--</div></div>
</div>
</div>
<div class="dash-settings">
<div class="chip-hint" style="margin-top:0">Tap a device status to open its settings</div>
<div class="out" id="outs"></div>
<button type="button" class="btn-ovr" id="btnOvr">Light override (OVR)</button>
<div class="navfoot">
  <a href="/dev/settings" class="btn-nav">Settings</a>
  <a href="/sys" class="btn-nav">System Info</a>
</div>
</div>
<script>
)rawliteral";
#endif

static const char WIFI_PAGE_DASH_JS[] PROGMEM = R"rawliteral(
function fmtTemp(v){return v===null?'ERR':v.toFixed(1)+'°C';}
function fmtHum(v){return v===null?'ERR':v+'%';}
function fmtUptime(j){return j.uptime_fmt||(j.uptime+' s');}
function modeName(m){return ['AUT','ON','OFF','MAN'][m]||'?';}
async function loadStatus(){
  try{
    const j=await fetchJson('/api/status');
    document.getElementById('meta').textContent=j.firmware+' | '+fmtUptime(j);
    document.getElementById('inc').textContent=fmtTemp(j.sensors.inc_c);
    document.getElementById('inc').className='val'+(j.sensors.inc_valid?'':' err');
    document.getElementById('grhT').textContent=fmtTemp(j.sensors.grh_c);
    document.getElementById('grhT').className='val'+(j.sensors.grh_valid?'':' err');
    document.getElementById('grhH').textContent=fmtHum(j.sensors.hum_pct);
    document.getElementById('grhH').className='val'+(j.sensors.hum_valid?'':' err');
    document.getElementById('clk').textContent=j.rtc.ok?j.rtc.time+' '+j.rtc.date:'RTC err';
    const ovrBtn=document.getElementById('btnOvr');
    ovrBtn.classList.toggle('on',!!j.outputs.light_override);
    ovrBtn.textContent=j.outputs.light_override?'Light OVR: ON (tap to off)':'Light override (OVR)';
    let h='';
    const o=j.outputs;
    for(const [lbl,k,path] of [['GRH','heat','heat'],['HUM','hum','hum'],['FAN','fan','fan'],['LGT','light','light'],['INC','inc','inc']]){
      h+='<a href="/dev/'+path+'" class="chip'+(o[k+'_on']?' on':'')+'">'+lbl+
         '<div class="m">'+modeName(o[k+'_mode'])+(k==='light'&&o.light_override?' OVR':'')+'</div></a>';
    }
    document.getElementById('outs').innerHTML=h;
  }catch(e){document.getElementById('meta').textContent='No data';}
}
document.getElementById('btnOvr').onclick=async()=>{
  try{
    await postForm({cmd:'light_override'});
    loadStatus();
  }catch(e){setConn(false);}
};
loadStatus();
setInterval(loadStatus,2000);
setInterval(checkConn,3000);
</script>
</body>
</html>
)rawliteral";

static const char WIFI_PAGE_DEV_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings — Psi Unit</title>
<style>
)rawliteral";

static const char WIFI_PAGE_DEV_BODY[] PROGMEM = R"rawliteral(
</style>
</head>
<body>
<div id="connBanner" class="conn-lost">No connection to device</div>
<h1 id="title">Settings</h1>
<div class="st-live" id="live">--</div>
<form id="form"></form>
<div id="extras"></div>
<div class="btnrow" id="btnRow">
  <button type="button" class="btn btn-save" id="btnSave">Save</button>
  <button type="button" class="btn btn-cancel" id="btnCancel">Cancel</button>
  <a href="/" class="btn btn-home">Home</a>
</div>
<div class="msg" id="msg"></div>
<div id="wheelModal" class="wheel-modal" aria-hidden="true">
  <div class="wheel-sheet">
    <div id="wheelModalTitle" class="wheel-sheet-title"></div>
    <div class="wheel-wrap">
      <div class="wheel-band"></div>
      <div class="wheel-col" id="wheelModalCol"></div>
    </div>
    <div class="wheel-sheet-ft">
      <button type="button" class="btn btn-cancel" id="wheelModalCancel">Cancel</button>
      <button type="button" class="btn btn-save" id="wheelModalDone">Done</button>
    </div>
  </div>
</div>
<script>
)rawliteral";

static const char WIFI_PAGE_DEV_JS[] PROGMEM = R"rawliteral(
const MODES3=[['0','AUTO'],['1','ON'],['2','OFF']];
const MODES4=[['0','AUTO'],['1','ON'],['2','OFF'],['3','MAN']];
const UNIT=[['0','Sec'],['1','min']];
const WIFI_TMO=[['0','Never'],['30','30m'],['60','60m'],['120','120m'],['240','240m']];
const DEV={
  heat:{title:'GRH heater',out:'heat',sections:[
    {title:'Operating mode',hint:'AUTO: on/off by temperature setpoint. Tap a mode to save.',fields:[
      {id:'heat_mode',label:'Mode',hint:'AUTO: on/off by temperature setpoint.',type:'mode',opts:MODES3}]},
    {title:'Temperature control',fields:[
      {id:'t_target',label:'Target temperature (°C)',hint:'Desired air temperature.',type:'deci',min:22,max:30,step:0.1},
      {id:'t_hyst',label:'Hysteresis (°C)',hint:'Dead band around target before switching.',type:'hyst',min:0.1,max:5,step:0.1}]}
  ]},
  hum:{title:'Humidifier (HUM)',out:'hum',sections:[
    {title:'Operating mode',hint:'MAN runs the cycle timer only. Tap a mode to save.',fields:[
      {id:'hum_mode',label:'Mode',hint:'MAN: cycle timer only (no humidity AUTO).',type:'mode',opts:MODES4}]},
    {title:'Humidity control',fields:[
      {id:'h_target',label:'Target humidity (%)',type:'int',min:50,max:100},
      {id:'h_hyst',label:'Hysteresis (%)',hint:'Dead band around target.',type:'int',min:1,max:20}]},
    {title:'Cycle timer',hint:'Pulse/rest timing for the humidifier.',fields:[
      {id:'hum_max',label:'Max run time (min)',type:'int',min:1,max:250,fullRow:1},
      {id:'hum_work',label:'On duration',type:'int',min:1,max:250},
      {id:'hum_work_unit',label:'On unit',type:'choice',opts:UNIT},
      {id:'hum_rest',label:'Off duration',type:'int',min:1,max:250},
      {id:'hum_rest_unit',label:'Off unit',type:'choice',opts:UNIT}]}
  ]},
  fan:{title:'Fan (FAN)',out:'fan',sections:[
    {title:'Operating mode',hint:'Tap a mode to save.',fields:[
      {id:'fan_mode',label:'Mode',type:'mode',opts:MODES3}]},
    {title:'Cycle timer',hint:'Periodic fan bursts in AUTO mode.',fields:[
      {id:'fan_work',label:'On duration',type:'int',min:1,max:250},
      {id:'fan_work_unit',label:'On time unit',type:'choice',opts:UNIT},
      {id:'fan_rest',label:'Off duration',type:'int',min:1,max:250},
      {id:'fan_rest_unit',label:'Off time unit',type:'choice',opts:UNIT}]}
  ]},
  light:{title:'Light (LGT)',out:'light',sections:[
    {title:'Operating mode',hint:'AUTO uses schedule below. Tap a mode to save.',fields:[
      {id:'light_mode',label:'Mode',hint:'AUTO uses schedule below.',type:'mode',opts:MODES3}]},
    {title:'Daily schedule',fields:[
      {id:'light_hour',label:'Start hour (0–23)',type:'int',min:0,max:23},
      {id:'light_duration',label:'Duration (hours)',type:'int',min:1,max:24},
      {id:'light_brightness',label:'PWM brightness (%)',hint:'Open picker, then Save.',type:'pct10wheel',min:0,max:100}]}
  ]},
  inc:{title:'INC heater',out:'inc',sections:[
    {title:'Operating mode',hint:'Tap a mode to save.',fields:[
      {id:'inc_mode',label:'Mode',type:'mode',opts:MODES3}]},
    {title:'Temperature control',fields:[
      {id:'inc_target',label:'Target temperature (°C)',type:'deci',min:22,max:30,step:0.1},
      {id:'inc_hyst',label:'Hysteresis (°C)',type:'hyst',min:0.1,max:5,step:0.1}]}
  ]},
  settings:{title:'Settings',hub:1,links:[
    {href:'/dev/clock',label:'Clock & date',desc:'Sync RTC from your phone'},
    {href:'/dev/display',label:'Display',desc:'OLED brightness and screen sleep'},
    {href:'/dev/rgb',label:'RGB status LED',desc:'Status LED on/off and brightness'},
    {href:'/dev/wifi',label:'Wi-Fi',desc:'Access point and session timeout'},
    {href:'/dev/log',label:'Sensor history log',desc:'Logging interval, enable/disable, clear data'}
  ],extras:[{cmd:'reset_settings',label:'Factory reset all settings',danger:1}]},
  log:{title:'Sensor history log',custom:'log'},
  clock:{title:'Clock & date',custom:'clock'},
  display:{title:'Display',sections:[
    {title:'OLED screen',fields:[
      {id:'sleep_min',label:'Screen sleep (minutes)',hint:'0 = always on. After inactivity the display turns off (screensaver).',type:'int',min:0,max:60},
      {id:'brightness',label:'OLED brightness (%)',hint:'Open picker, then Save.',type:'pct5wheel',min:0,max:100}]},
    {title:'Orientation',fields:[
      {id:'display_rotated',label:'Rotate 180°',type:'toggle'},
      {id:'display_inverted',label:'Invert colors',type:'toggle'}]}
  ]},
  rgb:{title:'RGB status LED',sections:[
    {title:'Status indicator',hint:'Built-in RGB LED shows system state (OK / Wi-Fi / error).',fields:[
      {id:'rgb_led_enabled',label:'RGB LED enabled',type:'toggle'},
      {id:'rgb_brightness',label:'RGB brightness (%)',hint:'Open picker, then Save.',type:'pct10wheel',min:0,max:100}]}
  ]},
  wifi:{title:'Wi-Fi',custom:'wifi',sections:[
    {title:'Wi-Fi Soft Access Point',fields:[
      {id:'wifi_ap_enabled',label:'Wi-Fi Soft Access Point',
        hint:'When on, the device creates its own Wi-Fi network (SoftAP). Connect your phone to PsiUnit-XXXX to open settings in the browser. This is not a connection to your home router.',type:'toggle'}]},
    {title:'Session',fields:[
      {id:'wifi_timeout',label:'Soft Access Point auto-off timeout',
        hint:'Never = hotspot stays on until you turn SoftAP off. Tap a value to save.',type:'choice',opts:WIFI_TMO,rowClass:'wifi-tmo-row'}]}
  ]}
};
let baseline={},allSettings={},devId='',devCfg=null;
function allFields(){
  if(!devCfg||!devCfg.sections)return[];
  return devCfg.sections.flatMap(s=>s.fields||[]);
}
function pageNeedsSaveBtn(){
  const fields=allFields();
  return fields.length>0&&fields.some(f=>!needsInstantSave(f));
}
function hideSaveRowIfInstantOnly(){
  if(pageNeedsSaveBtn())return;
  document.getElementById('btnSave').style.display='none';
  document.getElementById('btnCancel').style.display='none';
  document.getElementById('btnRow').style.gridTemplateColumns='1fr';
  const home=document.querySelector('.btn-home');
  if(home)home.style.gridColumn='1';
}
function devFromPath(){
  const p=location.pathname.split('/').filter(Boolean);
  if(p[0]!=='dev')return '';
  return p[1]||'';
}
function pad2(n){return String(n).padStart(2,'0');}
function deciUi(v){return(v/10).toFixed(1)+'\u00B0C';}
function hystUi(v){return(v/10).toFixed(1)+'\u00B0C';}
function pctStep(f){
  if(f.type==='pct10'||f.type==='pct10wheel')return 10;
  if(f.type==='pct5'||f.type==='pct5wheel')return 5;
  return 5;
}
function pctOpts(f){
  const step=pctStep(f),min=f.min??0,max=f.max??100,o=[];
  for(let p=min;p<=max;p+=step)o.push([String(p),p+'%']);
  return o;
}
function snapPct(f,v){
  const step=pctStep(f),min=f.min??0,max=f.max??100;
  let n=parseInt(v,10);
  if(isNaN(n))n=min;
  n=Math.max(min,Math.min(max,n));
  return String(Math.round(n/step)*step);
}
function rawFromField(f,ui){
  if(f.type==='deci')return String(Math.round(parseFloat(ui)*10));
  if(f.type==='hyst')return String(Math.round(parseFloat(ui)*10));
  if(f.type==='pct10'||f.type==='pct5'||f.type==='pct10wheel'||f.type==='pct5wheel')return snapPct(f,ui);
  return String(ui);
}
function uiFromRaw(f,raw){
  if(f.type==='deci')return deciUi(raw);
  if(f.type==='hyst')return hystUi(raw);
  if(f.type==='pct10'||f.type==='pct5'||f.type==='pct10wheel'||f.type==='pct5wheel')return snapPct(f,raw);
  return String(raw);
}
function getRtc(s){
  return s.rtc&&s.rtc.ok?{rtc_year:s.rtc.year,rtc_month:s.rtc.month,rtc_day:s.rtc.day,
    rtc_hour:s.rtc.hour,rtc_minute:s.rtc.minute,rtc_second:s.rtc.second||0}:{};
}
function rtcToDatetime(rtc){
  return rtc.rtc_year+'-'+pad2(rtc.rtc_month)+'-'+pad2(rtc.rtc_day)+'T'+pad2(rtc.rtc_hour)+':'+pad2(rtc.rtc_minute);
}
function readForm(){
  const o={};
  for(const f of allFields()){
    const el=document.getElementById(f.id);
    if(el)o[f.id]=el.value;
  }
  return o;
}
function writeForm(src){
  for(const f of allFields()){
    if(src[f.id]!=null)updateFieldDisplay(f,src[f.id]);
  }
}
function fieldOpts(f){
  if(f.opts)return f.opts;
  if(f.type==='pct10'||f.type==='pct5')return pctOpts(f);
  return [];
}
function highlightChoice(grpId,val){
  const g=document.getElementById(grpId);
  if(!g)return;
  g.querySelectorAll('.opt-btn').forEach(b=>{b.classList.toggle('on',b.dataset.v===String(val));});
}
function updateToggleBtn(f,val){
  const btn=document.getElementById('tog_'+f.id);
  const hid=document.getElementById(f.id);
  if(!btn||!hid)return;
  const on=String(val)==='1';
  hid.value=on?'1':'0';
  btn.classList.toggle('on',on);
  btn.textContent=f.label+(on?' — ON':' — OFF');
}
function fieldUsesWheel(f){
  return f.type==='int'||f.type==='deci'||f.type==='hyst'||f.type==='pct10wheel'||f.type==='pct5wheel';
}
function fieldFullWidth(f){
  if(f.fullRow)return true;
  if(f.type==='mode'||f.type==='toggle')return true;
  if(f.type==='choice'||f.type==='pct10'||f.type==='pct5'){
    const n=(f.opts||fieldOpts(f)).length;
    return n>3||!!f.rowClass;
  }
  return false;
}
const WHEEL_H=44;
let wheelModalField=null,wheelModalDraft=null,wheelScrollLock=false,wheelSelIdx=-1,wheelSnapTimer=0;
function wheelLabel(f,ui){
  if(f.type==='deci'||f.type==='hyst')return ui.replace('\u00B0C','\u00B0');
  if(f.id==='h_target'||f.id==='h_hyst')return ui+'%';
  if(f.id==='light_hour'||f.id==='light_duration')return ui+' h';
  if(f.id==='sleep_min')return ui==='0'?'Off':ui+' min';
  if(f.id==='hum_max')return ui+' min';
  if(f.type==='pct10wheel'||f.type==='pct5wheel')return ui+'%';
  if(f.id==='hum_work'||f.id==='hum_rest'||f.id==='fan_work'||f.id==='fan_rest')return ui;
  return ui;
}
function wheelEntries(f){
  const out=[];
  if(f.type==='deci'||f.type==='hyst'){
    const min=f.min??0,max=f.max??10,step=f.step??0.1;
    for(let x=min;x<=max+0.0001;x+=step){
      const raw=Math.round(x*10);
      const ui=uiFromRaw(f,raw);
      out.push({v:ui,label:wheelLabel(f,ui)});
    }
  }else if(f.type==='pct10wheel'||f.type==='pct5wheel'){
    const step=pctStep(f),min=f.min??0,max=f.max??100;
    for(let p=min;p<=max;p+=step){
      const ui=String(p);
      out.push({v:ui,label:wheelLabel(f,ui)});
    }
  }else{
    const min=f.min??0,max=f.max??100;
    for(let i=min;i<=max;i++){
      const ui=String(i);
      out.push({v:ui,label:wheelLabel(f,ui)});
    }
  }
  return out;
}
function updateValuePickerBtn(f){
  const btn=document.getElementById('pick_'+f.id);
  const hid=document.getElementById(f.id);
  if(!btn||!hid)return;
  btn.textContent=wheelLabel(f,hid.value);
}
function refreshAllValuePickers(){
  for(const f of allFields()){
    if(fieldUsesWheel(f))updateValuePickerBtn(f);
  }
}
function wheelModalCol(){return document.getElementById('wheelModalCol');}
function wheelModalItems(col){return col.querySelectorAll('.wheel-item');}
function wheelModalSyncPads(col){
  const padH=Math.max(0,Math.floor((col.clientHeight-WHEEL_H)/2));
  col.querySelectorAll('.wheel-pad').forEach(p=>{p.style.height=padH+'px';});
}
function wheelModalColCenter(col){return col.scrollTop+col.clientHeight/2;}
function wheelModalItemCenter(el){return el.offsetTop+WHEEL_H/2;}
function wheelModalPickIdx(col){
  const items=wheelModalItems(col);
  if(!items.length)return 0;
  const mid=wheelModalColCenter(col);
  let best=0,bestD=1e12;
  for(let i=0;i<items.length;i++){
    const d=Math.abs(wheelModalItemCenter(items[i])-mid);
    if(d<bestD){bestD=d;best=i;}
  }
  return best;
}
function wheelModalScrollToIdx(col,idx){
  const items=wheelModalItems(col);
  if(!items.length)return;
  if(idx<0)idx=0;
  if(idx>=items.length)idx=items.length-1;
  col.scrollTop=items[idx].offsetTop-(col.clientHeight-WHEEL_H)/2;
}
function wheelModalApplyIdx(col,idx){
  const items=wheelModalItems(col);
  if(!items.length)return;
  wheelSelIdx=idx;
  items.forEach((it,i)=>{it.classList.toggle('sel',i===idx);});
  wheelModalDraft=items[idx].dataset.v;
}
function wheelModalHighlight(){
  if(wheelScrollLock||!wheelModalField)return;
  const col=wheelModalCol();
  if(!col)return;
  const idx=wheelModalPickIdx(col);
  if(idx===wheelSelIdx)return;
  wheelModalApplyIdx(col,idx);
}
function wheelModalSnap(){
  if(wheelScrollLock||!wheelModalField)return;
  const col=wheelModalCol();
  if(!col)return;
  const items=wheelModalItems(col);
  if(!items.length)return;
  const idx=wheelModalPickIdx(col);
  wheelScrollLock=true;
  wheelModalScrollToIdx(col,idx);
  wheelModalApplyIdx(col,idx);
  requestAnimationFrame(()=>{wheelScrollLock=false;});
}
function wheelModalScrollTo(val){
  const col=wheelModalCol();
  if(!col)return;
  const items=wheelModalItems(col);
  let idx=0;
  for(let i=0;i<items.length;i++){
    if(items[i].dataset.v===String(val)){idx=i;break;}
  }
  wheelSelIdx=-1;
  wheelScrollLock=true;
  wheelModalScrollToIdx(col,idx);
  wheelModalApplyIdx(col,idx);
  requestAnimationFrame(()=>{wheelScrollLock=false;});
}
function wheelModalBuild(f){
  const col=wheelModalCol();
  if(!col)return;
  col.replaceChildren();
  const pad1=document.createElement('div');
  pad1.className='wheel-pad';
  col.appendChild(pad1);
  for(const e of wheelEntries(f)){
    const it=document.createElement('div');
    it.className='wheel-item';
    it.dataset.v=e.v;
    it.textContent=e.label;
    col.appendChild(it);
  }
  const pad2=document.createElement('div');
  pad2.className='wheel-pad';
  col.appendChild(pad2);
  wheelModalSyncPads(col);
}
function openWheelModal(f){
  const hid=document.getElementById(f.id);
  if(!hid)return;
  wheelModalField=f;
  wheelModalDraft=hid.value;
  document.getElementById('wheelModalTitle').textContent=f.label;
  wheelModalBuild(f);
  const modal=document.getElementById('wheelModal');
  modal.classList.add('show');
  modal.setAttribute('aria-hidden','false');
  requestAnimationFrame(()=>{
    const col=wheelModalCol();
    if(col)wheelModalSyncPads(col);
    requestAnimationFrame(()=>wheelModalScrollTo(hid.value));
  });
}
function closeWheelModal(apply){
  const modal=document.getElementById('wheelModal');
  modal.classList.remove('show');
  modal.setAttribute('aria-hidden','true');
  if(apply&&wheelModalField&&wheelModalDraft!=null){
    const hid=document.getElementById(wheelModalField.id);
    if(hid)hid.value=wheelModalDraft;
    updateValuePickerBtn(wheelModalField);
  }
  wheelModalField=null;
  wheelModalDraft=null;
  wheelSelIdx=-1;
  if(wheelSnapTimer){clearTimeout(wheelSnapTimer);wheelSnapTimer=0;}
  const col=wheelModalCol();
  if(col)col.replaceChildren();
}
function initWheelModal(){
  const col=wheelModalCol();
  if(!col)return;
  col.addEventListener('scroll',()=>{
    if(wheelScrollLock)return;
    wheelModalHighlight();
    if(wheelSnapTimer)clearTimeout(wheelSnapTimer);
    wheelSnapTimer=setTimeout(()=>{wheelSnapTimer=0;wheelModalSnap();},150);
  },{passive:true});
  document.getElementById('wheelModalDone').onclick=()=>closeWheelModal(true);
  document.getElementById('wheelModalCancel').onclick=()=>closeWheelModal(false);
  document.getElementById('wheelModal').onclick=(e)=>{
    if(e.target.id==='wheelModal')closeWheelModal(false);
  };
}
function appendValuePicker(d,f){
  appendHidden(d,f);
  const btn=document.createElement('button');
  btn.type='button';
  btn.className='val-btn';
  btn.id='pick_'+f.id;
  btn.onclick=()=>openWheelModal(f);
  d.appendChild(btn);
  updateValuePickerBtn(f);
}
function updateFieldDisplay(f,val){
  const hid=document.getElementById(f.id);
  if(hid)hid.value=val;
  if(f.type==='mode'||f.type==='choice'||f.type==='pct10'||f.type==='pct5'){
    highlightChoice('grp_'+f.id,val);
  }else if(f.type==='toggle'){
    updateToggleBtn(f,val);
  }else if(fieldUsesWheel(f)){
    updateValuePickerBtn(f);
  }
}
function needsInstantSave(f){
  return f.type==='mode'||f.type==='toggle'||f.type==='choice'||f.type==='pct10'||f.type==='pct5';
}
async function saveFieldInstant(f,uiVal){
  const msg=document.getElementById('msg');
  if(String(baseline[f.id])===String(uiVal))return;
  msg.textContent='Saving...';
  try{
    const j=await postForm({field:f.id,value:rawFromField(f,uiVal)});
    if(!j.ok)throw new Error(f.id);
    baseline[f.id]=uiVal;
    updateFieldDisplay(f,uiVal);
    msg.textContent='Saved';
    if(f.id==='wifi_ap_enabled'&&devCfg&&devCfg.custom==='wifi')await loadWifiInfo();
    if(devCfg&&devCfg.out)updateLive();
  }catch(e){msg.textContent='Save failed';setConn(false);}
}
function appendHidden(d,f){
  const hid=document.createElement('input');
  hid.type='hidden';hid.id=f.id;hid.value='0';
  d.appendChild(hid);
  return hid;
}
function appendChoiceRow(d,f){
  appendHidden(d,f);
  const row=document.createElement('div');
  row.className='opt-row'+(f.rowClass?' '+f.rowClass:'');
  row.id='grp_'+f.id;
  const instant=needsInstantSave(f);
  for(const [v,t] of fieldOpts(f)){
    const b=document.createElement('button');
    b.type='button';b.className='opt-btn';b.dataset.v=v;b.textContent=t;
    b.onclick=async()=>{
      if(document.getElementById(f.id).value===v)return;
      updateFieldDisplay(f,v);
      if(instant)await saveFieldInstant(f,v);
    };
    row.appendChild(b);
  }
  d.appendChild(row);
}
function appendToggle(d,f){
  appendHidden(d,f);
  const btn=document.createElement('button');
  btn.type='button';btn.className='toggle-btn';btn.id='tog_'+f.id;
  btn.onclick=async()=>{
    const hid=document.getElementById(f.id);
    const next=hid.value==='1'?'0':'1';
    updateFieldDisplay(f,next);
    await saveFieldInstant(f,next);
  };
  d.appendChild(btn);
  updateToggleBtn(f,'0');
}
function formatClockParts(rtc){
  if(!rtc||!rtc.rtc_year)return null;
  const sec=rtc.rtc_second!=null?rtc.rtc_second:0;
  return pad2(rtc.rtc_day)+'/'+pad2(rtc.rtc_month)+'/'+rtc.rtc_year+' '+
    pad2(rtc.rtc_hour)+':'+pad2(rtc.rtc_minute)+':'+pad2(sec);
}
function updatePhoneClockDisplay(){
  const el=document.getElementById('phoneClock');
  if(!el)return;
  const n=new Date();
  el.textContent=pad2(n.getDate())+'/'+pad2(n.getMonth()+1)+'/'+n.getFullYear()+' '+
    pad2(n.getHours())+':'+pad2(n.getMinutes())+':'+pad2(n.getSeconds());
}
async function updateDeviceClockDisplay(){
  const el=document.getElementById('devClock');
  if(!el)return;
  try{
    const j=await fetchJson('/api/settings');
    const t=formatClockParts(getRtc(j));
    el.textContent=t||'RTC not available';
  }catch(e){el.textContent='--';}
}
function showSyncOk(){
  const b=document.getElementById('syncOk');
  if(!b)return;
  b.classList.add('show');
  setTimeout(()=>b.classList.remove('show'),5000);
}
function buildCustomClock(form){
  const h=document.createElement('div');
  h.className='sect';h.textContent='Clock sync';
  form.appendChild(h);
  const p=document.createElement('div');
  p.className='sect-hint';
  p.textContent='Phone time uses your local clock (no extra permissions). Device time comes from the RTC chip.';
  form.appendChild(p);
  const phoneBox=document.createElement('div');
  phoneBox.className='clk-block';
  phoneBox.innerHTML='<div class="clk-lbl">Phone time</div><div id="phoneClock" class="clk-val">--</div>';
  form.appendChild(phoneBox);
  const devBox=document.createElement('div');
  devBox.className='clk-block';
  devBox.innerHTML='<div class="clk-lbl">Device time (RTC)</div><div id="devClock" class="clk-val">--</div>';
  form.appendChild(devBox);
  const ok=document.createElement('div');
  ok.id='syncOk';ok.className='sync-ok';ok.textContent='Clock synced successfully';
  form.appendChild(ok);
  const b=document.createElement('button');
  b.type='button';b.className='btn-extra';b.textContent='Sync from this phone';
  b.onclick=async()=>{
    const msg=document.getElementById('msg');
    msg.textContent='';
    try{
      const n=new Date();
      const j=await postForm({cmd:'set_rtc',year:n.getFullYear(),month:n.getMonth()+1,
        day:n.getDate(),hour:n.getHours(),minute:n.getMinutes(),second:n.getSeconds()});
      if(!j.ok)throw new Error('rtc');
      showSyncOk();
      msg.textContent='';
      await updateDeviceClockDisplay();
    }catch(e){msg.textContent='Sync failed';setConn(false);}
  };
  form.appendChild(b);
}
async function loadWifiInfo(){
  const box=document.getElementById('wifiInfo');
  if(!box)return;
  try{
    const j=await fetchJson('/api/wifi');
    let h='<div class="sect">Soft Access Point credentials</div>';
    h+='<div class="sect-hint">Connect to the PsiUnit Wi-Fi network, then scan the QR code or enter SSID and password manually on your phone or laptop.</div>';
    h+='<div class="wifi-box">';
    h+='<div class="wifi-row"><b>Network name (SSID)</b><div class="wifi-val">'+j.ssid+'</div></div>';
    h+='<div class="wifi-row"><b>Password</b><div class="wifi-val">Use QR below or OLED SET WiFi QR</div></div>';
    h+='<div class="wifi-row"><b>IP address</b><div class="wifi-val">'+(j.ip||'--')+'</div></div>';
    h+='<div class="wifi-row"><b>Soft Access Point status</b><div class="wifi-val">'+(j.active?'Active':'Inactive')+'</div></div>';
    h+='</div>';
    if(j.active && j.qr_url){
      let qrSrc=j.qr_url;
      if(typeof psiGetToken==='function'){
        const t=psiGetToken();
        if(t)qrSrc=j.qr_url+'?t='+encodeURIComponent(t);
      }
      h+='<div class="wifi-qr"><img src="'+qrSrc+'" alt="Wi-Fi QR code"></div>';
    }else if(j.active){
      h+='<div class="sect-hint">QR code unavailable.</div>';
    }else{
      h+='<div class="sect-hint">Tap Wi-Fi Soft Access Point above to enable and show the QR code.</div>';
    }
    box.innerHTML=h;
  }catch(e){
    box.innerHTML='<div class="sect-hint">Could not load network info.</div>';
  }
}
function buildCustomWifi(form){
  const intro=document.createElement('div');
  intro.className='sect-hint';
  intro.textContent='The device runs a Wi-Fi Soft Access Point (SoftAP): a local hotspot for phone or laptop access. It does not join your home Wi-Fi router.';
  form.appendChild(intro);
  const box=document.createElement('div');
  box.id='wifiInfo';
  form.appendChild(box);
}
let logEnabled='0',logInterval='5';
function updateLogToggleUi(){
  const btn=document.getElementById('tog_logEnabled');
  if(!btn)return;
  const on=logEnabled==='1';
  btn.classList.toggle('on',on);
  btn.textContent='Logging enabled'+(on?' — ON':' — OFF');
}
function highlightLogInterval(val){
  const g=document.getElementById('grp_logInterval');
  if(!g)return;
  g.querySelectorAll('.opt-btn').forEach(b=>{b.classList.toggle('on',b.dataset.v===String(val));});
}
async function saveLogSettings(){
  const msg=document.getElementById('msg');
  msg.textContent='Saving...';
  try{
    await psiPostUrl('/api/log',{enabled:logEnabled,interval_min:logInterval});
    msg.textContent='Saved';
    await loadLogConfig();
  }catch(e){msg.textContent='Save failed';setConn(false);}
}
function buildCustomLog(form){
  const h=document.createElement('div');
  h.className='sect';h.textContent='Data logging';
  form.appendChild(h);
  const p=document.createElement('div');
  p.className='sect-hint';
  p.textContent='Stores sensor readings on device flash (~30 days at 5 min). Tap to save.';
  form.appendChild(p);
  const enWrap=document.createElement('div');
  enWrap.className='fld';
  const enLb=document.createElement('label');
  enLb.textContent='Logging';
  enWrap.appendChild(enLb);
  const enBtn=document.createElement('button');
  enBtn.type='button';enBtn.className='toggle-btn';enBtn.id='tog_logEnabled';
  enBtn.onclick=async()=>{
    logEnabled=logEnabled==='1'?'0':'1';
    updateLogToggleUi();
    await saveLogSettings();
  };
  enWrap.appendChild(enBtn);
  form.appendChild(enWrap);
  const intWrap=document.createElement('div');
  intWrap.className='fld';
  const intLb=document.createElement('label');
  intLb.textContent='Record interval';
  intWrap.appendChild(intLb);
  const intRow=document.createElement('div');
  intRow.className='opt-row';intRow.id='grp_logInterval';
  for(const [v,t] of [['5','5m'],['10','10m'],['15','15m']]){
    const b=document.createElement('button');
    b.type='button';b.className='opt-btn';b.dataset.v=v;b.textContent=t;
    b.onclick=async()=>{
      if(logInterval===v)return;
      logInterval=v;
      highlightLogInterval(v);
      await saveLogSettings();
    };
    intRow.appendChild(b);
  }
  intWrap.appendChild(intRow);
  const intHint=document.createElement('div');
  intHint.className='fld-hint';
  intHint.textContent='Allowed range: 5–15 minutes.';
  intWrap.appendChild(intHint);
  form.appendChild(intWrap);
  const stat=document.createElement('div');
  stat.id='logStat';stat.className='sect-hint';
  form.appendChild(stat);
  const clrBtn=document.createElement('button');
  clrBtn.type='button';clrBtn.className='btn-dng';clrBtn.textContent='Clear all sensor logs';
  clrBtn.onclick=async()=>{
    if(!confirm('Delete all stored sensor history?'))return;
    const msg=document.getElementById('msg');
    msg.textContent='Clearing...';
    try{
      const j=await postForm({cmd:'clear_sensor_log'});
      msg.textContent=j.ok?'Logs cleared':'Failed';
      if(j.ok)await loadLogConfig();
    }catch(e){msg.textContent='Error';setConn(false);}
  };
  form.appendChild(clrBtn);
}
async function loadLogConfig(){
  const stat=document.getElementById('logStat');
  try{
    const j=await fetchJson('/api/log');
    if(!j.compiled_in){if(stat)stat.textContent='Sensor log module is not compiled in.';return;}
    logEnabled=j.enabled?'1':'0';
    logInterval=String(j.interval_min);
    updateLogToggleUi();
    highlightLogInterval(logInterval);
    if(stat){
      let s='Storage: '+(j.ready?'OK':'unavailable')+'. Retention ~'+j.retention_days+' days.';
      if(j.records){s+=' Points: INC '+j.records.inc+', GRH °C '+j.records.grh_t+', GRH % '+j.records.grh_h+'.';}
      stat.textContent=s;
    }
  }catch(e){if(stat)stat.textContent='Could not load log settings.';}
}
function buildForm(){
  const form=document.getElementById('form');
  form.innerHTML='';
  if(devCfg.hub){
    for(const l of devCfg.links||[]){
      const a=document.createElement('a');
      a.href=l.href;a.className='hub-link';
      a.innerHTML='<b>'+l.label+'</b><span>'+l.desc+'</span>';
      form.appendChild(a);
    }
    return;
  }
  if(devCfg.custom==='clock'){buildCustomClock(form);}
  if(devCfg.custom==='wifi'){buildCustomWifi(form);}
  if(devCfg.custom==='log'){buildCustomLog(form);}
  for(const sec of devCfg.sections||[]){
    const h=document.createElement('div');
    h.className='sect';h.textContent=sec.title;
    form.appendChild(h);
    if(sec.hint){
      const p=document.createElement('div');
      p.className='sect-hint';p.textContent=sec.hint;
      form.appendChild(p);
    }
    const grid=document.createElement('div');
    grid.className='fld-grid';
    for(const f of sec.fields){
      const d=document.createElement('div');
      d.className='fld'+(fieldFullWidth(f)?' fld-full':'');
      if(f.type!=='toggle'){
        const lb=document.createElement('label');
        lb.textContent=f.label;
        d.appendChild(lb);
      }
      if(f.type==='mode'||f.type==='choice'||f.type==='pct10'||f.type==='pct5'){
        appendChoiceRow(d,f);
      }else if(f.type==='toggle'){
        appendToggle(d,f);
      }else if(f.type==='datetime'){
        const el=document.createElement('input');el.id=f.id;el.type='datetime-local';
        d.appendChild(el);
      }else if(fieldUsesWheel(f)){
        appendValuePicker(d,f);
      }else{
        const el=document.createElement('input');el.id=f.id;el.type='number';
        if(f.min!=null)el.min=f.min;
        if(f.max!=null)el.max=f.max;
        if(f.step!=null)el.step=f.step;
        d.appendChild(el);
      }
      if(f.hint){
        const ht=document.createElement('div');
        ht.className='fld-hint';ht.textContent=f.hint;
        d.appendChild(ht);
      }
      grid.appendChild(d);
    }
    form.appendChild(grid);
  }
  const ex=document.getElementById('extras');
  ex.innerHTML='';
  if(devCfg.extras){
    for(const e of devCfg.extras){
      const b=document.createElement('button');
      b.type='button';
      b.className=e.danger?'btn-dng':'btn-extra';
      b.textContent=e.label;
      b.onclick=async()=>{
        if(e.danger&&!confirm('Reset all settings to factory defaults?'))return;
        try{
          const j=await postForm({cmd:e.cmd});
          document.getElementById('msg').textContent=j.ok?'Done':'Failed';
          if(j.ok&&!devCfg.hub)await reloadBaseline();
        }catch(err){document.getElementById('msg').textContent='Error';setConn(false);}
      };
      ex.appendChild(b);
    }
  }
}
function applyBaselineToForm(){writeForm(baseline);}
async function reloadBaseline(){
  allSettings=await fetchJson('/api/settings');
  const rtc=getRtc(allSettings);
  baseline={};
  for(const f of allFields()){
    if(f.type==='datetime')baseline[f.id]=rtc.rtc_year?rtcToDatetime(rtc):'';
    else baseline[f.id]=uiFromRaw(f,allSettings[f.id]??0);
  }
  applyBaselineToForm();
  refreshAllValuePickers();
}
async function updateLive(){
  if(!devCfg||!devCfg.out)return;
  try{
    const j=await fetchJson('/api/status');
    const o=j.outputs;const k=devCfg.out;
    const mn=['AUT','ON','OFF','MAN'];
    let s=(o[k+'_on']?'ON':'OFF')+' | '+(mn[o[k+'_mode']]||'?');
    if(k==='light'&&o.light_override)s+=' OVR';
    document.getElementById('live').textContent='Live: '+s;
  }catch(e){document.getElementById('live').textContent='Live: --';}
}
document.getElementById('btnCancel').onclick=()=>{
  applyBaselineToForm();
  refreshAllValuePickers();
  document.getElementById('msg').textContent='Changes discarded';
};
document.getElementById('btnSave').onclick=async()=>{
  const msg=document.getElementById('msg');
  msg.textContent='Saving...';
  const cur=readForm();
  try{
    for(const f of allFields()){
      if(needsInstantSave(f))continue;
      if(String(cur[f.id])===String(baseline[f.id]))continue;
      const j=await postForm({field:f.id,value:rawFromField(f,cur[f.id])});
      if(!j.ok)throw new Error(f.id);
    }
    await reloadBaseline();
    msg.textContent='Saved';
    if(devCfg.custom==='wifi')await loadWifiInfo();
  }catch(e){msg.textContent='Save failed';setConn(false);}
};
async function init(){
  devId=devFromPath();
  devCfg=DEV[devId];
  if(!devCfg){location.href='/';return;}
  document.body.classList.add('dev-page');
  initWheelModal();
  document.getElementById('title').textContent=devCfg.title;
  if(devCfg.hub||devCfg.custom==='clock'||devCfg.custom==='log'){
    document.getElementById('live').style.display='none';
    document.getElementById('btnSave').style.display='none';
    document.getElementById('btnCancel').style.display='none';
    document.getElementById('btnRow').style.gridTemplateColumns='1fr';
    const home=document.querySelector('.btn-home');
    if(home){home.style.gridColumn='1';}
  }
  buildForm();
  if(devCfg.custom==='clock'){
    updatePhoneClockDisplay();
    await updateDeviceClockDisplay();
    setInterval(updatePhoneClockDisplay,1000);
    setInterval(updateDeviceClockDisplay,2000);
  }else if(devCfg.custom==='wifi'){
    hideSaveRowIfInstantOnly();
    try{
      await reloadBaseline();
      await loadWifiInfo();
      document.getElementById('msg').textContent='';
    }catch(e){
      document.getElementById('msg').textContent='No data';
      setConn(false);
    }
  }else if(devCfg.custom==='log'){
    try{
      await loadLogConfig();
      document.getElementById('msg').textContent='';
    }catch(e){
      document.getElementById('msg').textContent='No data';
      setConn(false);
    }
  }else if(!devCfg.hub){
    hideSaveRowIfInstantOnly();
    try{
      await reloadBaseline();
      document.getElementById('msg').textContent='';
    }catch(e){
      document.getElementById('msg').textContent='No data';
      setConn(false);
    }
    if(devCfg.out){
      updateLive();
      setInterval(updateLive,2000);
    }
  }
  setInterval(checkConn,3000);
}
init();
</script>
</body>
</html>
)rawliteral";

static const char WIFI_PAGE_SYS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>System Info — Psi Unit</title>
<style>
)rawliteral";

static const char WIFI_PAGE_SYS_BODY[] PROGMEM = R"rawliteral(
</style>
</head>
<body>
<div id="connBanner" class="conn-lost">No connection to device</div>
<div class="btnrow" style="margin-top:0;margin-bottom:12px">
  <span></span><span></span>
  <a href="/" class="btn btn-home">Home</a>
</div>
<h1>System Info</h1>
<div class="meta" id="meta">Loading...</div>
<div id="rows"></div>
<script>
)rawliteral";

static const char WIFI_PAGE_SYS_JS[] PROGMEM = R"rawliteral(
function fmtUptime(j){return j.uptime_fmt||(j.uptime+' s');}
async function loadSys(){
  try{
    const j=await fetchJson('/api/sys');
    document.getElementById('meta').textContent=j.firmware+' | uptime '+fmtUptime(j);
    let h='';
    for(const x of j.rows){
      if(x.k==='s'){h+='<div class="sect">'+x.l+'</div>';}
      else if(x.k==='f'){h+='<div class="row"><span class="lbl">'+x.l+'</span><span class="rval">'+x.v+'</span></div>';}
    }
    document.getElementById('rows').innerHTML=h;
  }catch(e){document.getElementById('meta').textContent='No data';}
}
loadSys();
setInterval(loadSys,2000);
setInterval(checkConn,3000);
</script>
</body>
</html>
)rawliteral";

#if SENSOR_LOG_ENABLE
static const char WIFI_PAGE_HIST_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensor history — Psi Unit</title>
<style>
)rawliteral";

static const char WIFI_PAGE_HIST_BODY[] PROGMEM = R"rawliteral(
</style>
</head>
<body>
<div id="connBanner" class="conn-lost">No connection to device</div>
<div class="btnrow" style="margin-top:0;margin-bottom:12px">
  <span></span><span></span>
  <a href="/" class="btn btn-home">Home</a>
</div>
<h1 id="histTitle">Sensor history</h1>
<div class="meta" id="meta">Loading...</div>
<div class="chart-panel">
<div class="range-row" id="ranges"></div>
<div class="chart-meta" id="chartMeta"></div>
<div class="chart-wrap"><svg id="chart" viewBox="0 0 600 220" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="Sensor history chart" shape-rendering="geometricPrecision"></svg></div>
<div class="chart-empty" id="empty" style="display:none">No data for this period</div>
</div>
<script>
)rawliteral";

static const char WIFI_PAGE_HIST_JS[] PROGMEM = R"rawliteral(
const SERIES={inc:{title:'INC temperature',unit:'°C'},grh_t:{title:'GRH temperature',unit:'°C'},grh_h:{title:'GRH humidity',unit:'%'}};
const RANGES=[['6 h',6],['24 h',24],['7 d',168],['30 d',720]];
let hours=24;
function seriesId(){
  const p=location.pathname.split('/').filter(Boolean);
  return (p[0]==='hist'&&p[1])?p[1]:'';
}
const RTC_TZ={timeZone:'UTC'};
function fmtTs(ts){
  return new Date(ts*1000).toLocaleString(undefined,
    Object.assign({month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'},RTC_TZ));
}
function formatY(v,unit){
  if(unit==='%') return String(Math.round(v));
  return Number(v).toFixed(1);
}
function niceStep(range,targetTicks){
  if(range<=0) return 1;
  const rough=range/targetTicks;
  const exp=Math.floor(Math.log10(rough));
  const pow=Math.pow(10,exp);
  const x=rough/pow;
  let m=10;
  if(x<=1) m=1;
  else if(x<=2) m=2;
  else if(x<=5) m=5;
  return m*pow;
}
function computeYTicks(dataMin,dataMax,target){
  let minV=dataMin,maxV=dataMax;
  if(minV===maxV){minV-=1;maxV+=1;}
  const step=niceStep(maxV-minV,target);
  const lo=Math.floor(minV/step)*step;
  const hi=Math.ceil(maxV/step)*step;
  const ticks=[];
  for(let v=lo;v<=hi+step*1e-6;v+=step){
    ticks.push(Math.round(v*1000)/1000);
  }
  return {min:lo,max:hi,ticks};
}
function fmtTsAxis(ts,rangeHours){
  const d=new Date(ts*1000);
  if(rangeHours<=24){
    return d.toLocaleString(undefined,Object.assign({hour:'2-digit',minute:'2-digit'},RTC_TZ));
  }
  if(rangeHours<=168){
    return d.toLocaleString(undefined,Object.assign({weekday:'short',day:'numeric'},RTC_TZ));
  }
  return d.toLocaleString(undefined,Object.assign({month:'short',day:'numeric'},RTC_TZ));
}
function timeAxisStep(rangeHours){
  if(rangeHours<=6) return 3600;
  if(rangeHours<=24) return 14400;
  if(rangeHours<=168) return 86400;
  return 345600;
}
function alignTickStart(ts,stepSec){
  return Math.floor(ts/stepSec)*stepSec;
}
function buildTimeTicks(t0,t1,rangeHours){
  if(t1<=t0) return [t0];
  const step=timeAxisStep(rangeHours);
  const ticks=[];
  let t=alignTickStart(t0,step);
  while(t<t0) t+=step;
  while(t<=t1){ticks.push(t);t+=step;}
  return ticks;
}
const CHART_W=600,CHART_H=220;
const CHART_FONT='system-ui,sans-serif';
function svgEsc(s){
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function drawChart(points,unit,axisFrom,axisTo){
  const svg=document.getElementById('chart');
  const empty=document.getElementById('empty');
  if(!points||!points.length){
    svg.style.display='none';
    svg.innerHTML='';
    empty.style.display='block';
    document.getElementById('chartMeta').textContent='';
    return;
  }
  svg.style.display='block';
  empty.style.display='none';
  const w=CHART_W,h=CHART_H;
  const pad={l:46,r:14,t:14,b:32};
  let dataMin=points[0][1],dataMax=points[0][1];
  for(const p of points){
    if(p[1]<dataMin) dataMin=p[1];
    if(p[1]>dataMax) dataMax=p[1];
  }
  const yAxis=computeYTicks(dataMin,dataMax,5);
  const minV=yAxis.min,maxV=yAxis.max;
  const t0=(axisFrom!=null)?Number(axisFrom):points[0][0];
  const t1=(axisTo!=null)?Number(axisTo):points[points.length-1][0];
  const span=(t1>t0)?(t1-t0):1;
  const plotW=w-pad.l-pad.r,plotH=h-pad.t-pad.b;
  const xTicks=buildTimeTicks(t0,t1,hours);
  const yAt=(v)=>pad.t+plotH*(1-(v-minV)/(maxV-minV));
  const xAt=(ts)=>pad.l+Math.max(0,Math.min(plotW,((ts-t0)/span)*plotW));
  const g=[];
  g.push('<rect width="'+w+'" height="'+h+'" fill="#161b22"/>');
  for(const v of yAxis.ticks){
    const y=yAt(v);
    g.push('<line x1="'+pad.l+'" y1="'+y+'" x2="'+(w-pad.r)+'" y2="'+y+'" stroke="#21262d" stroke-width="1"/>');
    g.push('<text x="'+(pad.l-6)+'" y="'+y+'" fill="#8b949e" font-size="12" font-family="'+CHART_FONT+'" text-anchor="end" dominant-baseline="middle">'+svgEsc(formatY(v,unit))+'</text>');
  }
  for(const ts of xTicks){
    const x=xAt(ts);
    g.push('<line x1="'+x+'" y1="'+pad.t+'" x2="'+x+'" y2="'+(h-pad.b)+'" stroke="#21262d" stroke-width="1"/>');
    g.push('<text x="'+x+'" y="'+(h-8)+'" fill="#8b949e" font-size="12" font-family="'+CHART_FONT+'" text-anchor="middle">'+svgEsc(fmtTsAxis(ts,hours))+'</text>');
  }
  g.push('<polyline fill="none" stroke="#30363d" stroke-width="1" points="'+pad.l+','+pad.t+' '+pad.l+','+(h-pad.b)+' '+(w-pad.r)+','+(h-pad.b)+'"/>');
  const linePts=points.map(p=>xAt(p[0]).toFixed(1)+','+yAt(p[1]).toFixed(1)).join(' ');
  g.push('<polyline fill="none" stroke="#58a6ff" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" points="'+linePts+'"/>');
  svg.innerHTML=g.join('');
  document.getElementById('chartMeta').textContent=
    fmtTs(t0)+' – '+fmtTs(t1)+' · '+points.length+' pts · '+
    formatY(dataMin,unit)+' – '+formatY(dataMax,unit)+' '+unit;
}
function buildRanges(){
  const row=document.getElementById('ranges');
  row.innerHTML='';
  for(const [lbl,h] of RANGES){
    const b=document.createElement('button');
    b.type='button';b.className='range-btn'+(h===hours?' on':'');
    b.textContent=lbl;
    b.onclick=()=>{hours=h;buildRanges();loadChart();};
    row.appendChild(b);
  }
}
async function loadChart(){
  const id=seriesId();
  const cfg=SERIES[id];
  if(!cfg){location.href='/';return;}
  document.getElementById('histTitle').textContent=cfg.title;
  try{
    const j=await fetchJson('/api/history?series='+encodeURIComponent(id)+'&hours='+hours+'&points=400');
    document.getElementById('meta').textContent=j.ok?'':'Error';
    if(!j.ok){
      document.getElementById('empty').style.display='block';
      document.getElementById('empty').textContent=j.error==='disabled'?'Logging is disabled in settings':(j.error||'Error');
      document.getElementById('chart').style.display='none';
      return;
    }
    drawChart(j.points,cfg.unit,Number(j.from),Number(j.to));
  }catch(e){
    document.getElementById('meta').textContent='No data';
    document.getElementById('empty').style.display='block';
    document.getElementById('chart').style.display='none';
  }
}
buildRanges();
loadChart();
setInterval(loadChart,60000);
setInterval(checkConn,3000);
</script>
</body>
</html>
)rawliteral";
#endif

static void wifi_sendCompositePage(AsyncWebServerRequest *request,
                                   const char *part1, const char *part2,
                                   const char *css, const char *connJs,
                                   const char *part3, const char *part4) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(FPSTR(part1));
  response->print(FPSTR(css));
  response->print(FPSTR(part2));
  response->print(connJs);
  if (part3 != nullptr) {
    response->print(FPSTR(part3));
  }
  if (part4 != nullptr) {
    response->print(FPSTR(part4));
  }
  request->send(response);
}

static void wifi_webSendDevPage(AsyncWebServerRequest *request) {
  wifi_sendCompositePage(request, WIFI_PAGE_DEV_HTML, WIFI_PAGE_DEV_BODY, WIFI_WEB_CSS, wifi_connJs(),
                         WIFI_PAGE_DEV_JS, nullptr);
}

struct WifiFieldDef {
  const char *name;
  SettingsFieldId field;
  ItemType itemType;
  int16_t minv;
  int16_t maxv;
};

static const WifiFieldDef kWifiFields[] = {
    {"heat_mode", SF_HEAT_MODE, IT_MODE, 0, 2},
    {"hum_mode", SF_HUM_MODE, IT_MODE, 0, 3},
    {"fan_mode", SF_FAN_MODE, IT_MODE, 0, 2},
    {"light_mode", SF_LIGHT_MODE, IT_MODE, 0, 2},
    {"inc_mode", SF_INC_MODE, IT_MODE, 0, 2},
    {"t_target", SF_T_TARGET, IT_INT, TEMP_SET_MIN, TEMP_SET_MAX},
    {"inc_target", SF_INC_TARGET, IT_INT, TEMP_SET_MIN, TEMP_SET_MAX},
    {"t_hyst", SF_T_HYST, IT_HYST, TEMP_HYST_MIN, TEMP_HYST_MAX},
    {"inc_hyst", SF_INC_HYST, IT_HYST, TEMP_HYST_MIN, TEMP_HYST_MAX},
    {"h_target", SF_H_TARGET, IT_INT, HUM_SET_MIN, HUM_SET_MAX},
    {"h_hyst", SF_H_HYST, IT_INT, 1, 20},
    {"hum_max", SF_HUM_MAX, IT_INT, 1, 250},
    {"hum_work", SF_HUM_WORK, IT_INT, 1, 250},
    {"hum_rest", SF_HUM_REST, IT_INT, 1, 250},
    {"hum_work_unit", SF_HUM_WORK_UNIT, IT_UNIT, 0, 1},
    {"hum_rest_unit", SF_HUM_REST_UNIT, IT_UNIT, 0, 1},
    {"fan_work", SF_FAN_WORK, IT_INT, 1, 250},
    {"fan_rest", SF_FAN_REST, IT_INT, 1, 250},
    {"fan_work_unit", SF_FAN_WORK_UNIT, IT_UNIT, 0, 1},
    {"fan_rest_unit", SF_FAN_REST_UNIT, IT_UNIT, 0, 1},
    {"light_hour", SF_LIGHT_HOUR, IT_INT, 0, 23},
    {"light_duration", SF_LIGHT_DURATION, IT_INT, 1, 24},
    {"light_brightness", SF_LIGHT_BRIGHTNESS, IT_INT, 0, 100},
    {"sleep_min", SF_SLEEP_MIN, IT_INT, 0, 60},
    {"brightness", SF_BRIGHTNESS, IT_INT, 0, 100},
    {"display_rotated", SF_DISPLAY_ROTATED, IT_BOOL, 0, 1},
    {"display_inverted", SF_DISPLAY_INVERTED, IT_BOOL, 0, 1},
    {"rgb_led_enabled", SF_RGB_LED_ENABLED, IT_BOOL, 0, 1},
    {"rgb_brightness", SF_RGB_BRIGHTNESS, IT_INT, 0, 100},
    {"wifi_timeout", SF_WIFI_TIMEOUT, IT_INT, WIFI_AP_SESSION_NEVER, WIFI_AP_SESSION_MIN_MAX},
    {"wifi_ap_enabled", SF_WIFI_AP_ENABLED, IT_BOOL, 0, 1},
};

#if WIFI_WEB_AUTH_ENABLE
static char s_webSessionToken[33];
static unsigned long s_webSessionExpiryMs = 0;

void wifi_web_resetSession() {
  s_webSessionToken[0] = '\0';
  s_webSessionExpiryMs = 0;
}

static void wifi_webIssueSession() {
  static const char hex[] = "0123456789abcdef";
  for (uint8_t i = 0; i < 32; i++) {
    s_webSessionToken[i] = hex[esp_random() % 16U];
  }
  s_webSessionToken[32] = '\0';
  s_webSessionExpiryMs = millis() + WIFI_WEB_SESSION_MS;
}

static bool wifi_webValidateApPassword(const char *provided) {
  if (provided == nullptr) {
    return false;
  }
  char ssid[16];
  char pass[WIFI_AP_PASS_LEN + 1];
  if (!wifi_manager_getApCredentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
    return false;
  }
  return strncmp(provided, pass, WIFI_AP_PASS_LEN + 1) == 0;
}

static bool wifi_webIsAuthed(AsyncWebServerRequest *request) {
  if (s_webSessionToken[0] == '\0') {
    return false;
  }
  if (millisDeadlineReached(millis(), s_webSessionExpiryMs)) {
    wifi_web_resetSession();
    return false;
  }
  if (request->hasHeader("X-Psi-Session")) {
    return request->getHeader("X-Psi-Session")->value().equals(s_webSessionToken);
  }
  if (request->hasParam("t")) {
    return request->getParam("t")->value().equals(s_webSessionToken);
  }
  return false;
}

static bool wifi_webRequireAuth(AsyncWebServerRequest *request) {
  if (wifi_webIsAuthed(request)) {
    return true;
  }
  request->send(401, "application/json", F("{\"ok\":false,\"error\":\"auth\"}"));
  return false;
}
#else
void wifi_web_resetSession() {}

static bool wifi_webRequireAuth(AsyncWebServerRequest *request) {
  (void)request;
  return true;
}
#endif

static char s_wifiStatusJsonBuf[896];
static char s_wifiSettingsJsonBuf[1536];
static char s_wifiSysJsonBuf[2560];
static char s_wifiInfoJsonBuf[512];
static char s_wifiQrSvgBuf[12288];
#if WIFI_WEB_AUTH_ENABLE
static char s_wifiAuthJsonBuf[128];
#endif

static int wifi_jsonAppendIntField(char *buf, size_t cap, int off, const char *key, int32_t value) {
  return wifi_json_off(buf, cap, off, ",\"%s\":%ld", key, (long)value);
}

static const char *wifi_getSysJson() {
  system_info_refresh();
  int off = wifi_json_off(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), 0, "{\"firmware\":\"");
  off = wifi_json_append_escape(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, FIRMWARE_VERSION);
  off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, "\"");
  off = wifi_json_append_uptime(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, (uint32_t)(millis() / 1000UL));
  off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, ",\"rows\":[");
  if (off < 0) {
    return wifi_json_overflow_response();
  }

  bool first = true;
  const uint8_t count = system_info_rowCount();
  for (uint8_t i = 0; i < count; i++) {
    const SystemInfoRowKind kind = system_info_rowKind(i);
    if (kind == SYSINFO_ROW_DIVIDER) {
      continue;
    }
    if (!first) {
      off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, ",");
    } else {
      first = false;
    }
    off = wifi_json_off(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, "{\"k\":\"%c\",\"l\":\"",
                        (kind == SYSINFO_ROW_SECTION) ? 's' : 'f');
    off = wifi_json_append_escape(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, system_info_rowLabel(i));
    off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, "\"");
    if (kind == SYSINFO_ROW_FIELD) {
      off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, ",\"v\":\"");
      off = wifi_json_append_escape(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, system_info_rowValue(i));
      off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, "\"");
    }
    off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, "}");
    if (off < 0) {
      return wifi_json_overflow_response();
    }
  }
  off = wifi_json_append_cstr(s_wifiSysJsonBuf, sizeof(s_wifiSysJsonBuf), off, "]}");
  if (!wifi_json_valid(off, sizeof(s_wifiSysJsonBuf))) {
    return wifi_json_overflow_response();
  }
  return s_wifiSysJsonBuf;
}

static void wifi_buildStatusJson() {
  AppSnapshot snap;
  app_buildSnapshot(snap);

  const uint32_t uptimeSec = (uint32_t)(millis() / 1000UL);
  char uptimeFmt[SYSINFO_VALUE_CHARS + 1];
  system_info_formatUptimeSec(uptimeSec, uptimeFmt, sizeof(uptimeFmt));

  int off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), 0,
                          "{\"firmware\":\"%s\",\"uptime\":%lu,\"uptime_fmt\":\"%s\",\"nvs_ready\":%s",
                          FIRMWARE_VERSION, (unsigned long)uptimeSec, uptimeFmt, snap.nvsReady ? "true" : "false");

  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, ",\"rtc\":{\"ok\":%s",
                      snap.rtc.ok ? "true" : "false");
  if (snap.rtc.ok) {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off,
                        ",\"time\":\"%02u:%02u:%02u\",\"date\":\"%02u/%02u/%04u\"",
                        (unsigned)snap.rtc.hour, (unsigned)snap.rtc.minute, (unsigned)snap.rtc.second,
                        (unsigned)snap.rtc.day, (unsigned)snap.rtc.month, (unsigned)snap.rtc.year);
  }
  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "}");

  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, ",\"sensors\":{");
  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "\"inc_c\":");
  if (snap.incTemp.valid) {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "%.1f",
                        (double)snap.incTemp.value / 10.0);
  } else {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "null");
  }
  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, ",\"grh_c\":");
  if (snap.grhTemp.valid) {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "%.1f",
                        (double)snap.grhTemp.value / 10.0);
  } else {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "null");
  }
  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, ",\"hum_pct\":");
  if (snap.grhHum.valid) {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "%d", (int)snap.grhHum.value);
  } else {
    off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off, "null");
  }
  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off,
                      ",\"inc_valid\":%s,\"grh_valid\":%s,\"hum_valid\":%s}", snap.incTemp.valid ? "true" : "false",
                      snap.grhTemp.valid ? "true" : "false", snap.grhHum.valid ? "true" : "false");

  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off,
                      ",\"outputs\":{\"heat_on\":%s,\"hum_on\":%s,\"fan_on\":%s,\"light_on\":%s,\"inc_on\":%s,"
                      "\"light_override\":%s,\"heat_mode\":%u,\"hum_mode\":%u,\"fan_mode\":%u,\"light_mode\":%u,"
                      "\"inc_mode\":%u}",
                      snap.outputs.heatOn ? "true" : "false", snap.outputs.humOn ? "true" : "false",
                      snap.outputs.fanOn ? "true" : "false", snap.outputs.lightOn ? "true" : "false",
                      snap.outputs.incOn ? "true" : "false", snap.outputs.lightOverride ? "true" : "false",
                      (unsigned)snap.outputs.heatMode, (unsigned)snap.outputs.humMode,
                      (unsigned)snap.outputs.fanMode, (unsigned)snap.outputs.lightMode,
                      (unsigned)snap.outputs.incMode);

  off = wifi_json_off(s_wifiStatusJsonBuf, sizeof(s_wifiStatusJsonBuf), off,
                      ",\"setpoints\":{\"t_target_c\":%.1f,\"h_target\":%u}}",
                      (double)settings_fieldGetInt16(SF_T_TARGET) / 10.0,
                      (unsigned)settings_fieldGetUInt8(SF_H_TARGET));
  if (!wifi_json_valid(off, sizeof(s_wifiStatusJsonBuf))) {
    strncpy(s_wifiStatusJsonBuf, wifi_json_overflow_response(), sizeof(s_wifiStatusJsonBuf));
    s_wifiStatusJsonBuf[sizeof(s_wifiStatusJsonBuf) - 1] = '\0';
  }
}

static const char *wifi_getStatusJson() {
  wifi_buildStatusJson();
  return s_wifiStatusJsonBuf;
}

static const char *wifi_getSettingsJson() {
  AppSnapshot snap;
  app_buildSnapshot(snap);

  int off = wifi_json_off(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), 0, "{\"firmware\":\"");
  off = wifi_json_append_escape(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, FIRMWARE_VERSION);
  off = wifi_json_append_cstr(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, "\"");
  off = wifi_json_append_uptime(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off,
                                (uint32_t)(millis() / 1000UL));
  if (off < 0) {
    return wifi_json_overflow_response();
  }

  for (size_t i = 0; i < sizeof(kWifiFields) / sizeof(kWifiFields[0]); i++) {
    const WifiFieldDef &def = kWifiFields[i];
    if (settings_fieldIsTempDeci(def.field)) {
      off = wifi_jsonAppendIntField(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, def.name,
                                    settings_fieldGetInt16(def.field));
    } else if (settings_fieldIsBrightnessPercent(def.field)) {
      off = wifi_jsonAppendIntField(
          s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, def.name,
          settings_fieldBrightnessRawToPercent(settings_fieldGetUInt8(def.field), def.field));
    } else {
      off = wifi_jsonAppendIntField(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, def.name,
                                    settings_accessGetUInt8(def.field));
    }
    if (off < 0) {
      return wifi_json_overflow_response();
    }
  }

  off = wifi_json_off(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, ",\"rtc\":{\"ok\":%s",
                      snap.rtc.ok ? "true" : "false");
  if (snap.rtc.ok) {
    off = wifi_json_off(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off,
                        ",\"year\":%u,\"month\":%u,\"day\":%u,\"hour\":%u,\"minute\":%u,\"second\":%u",
                        (unsigned)snap.rtc.year, (unsigned)snap.rtc.month, (unsigned)snap.rtc.day,
                        (unsigned)snap.rtc.hour, (unsigned)snap.rtc.minute, (unsigned)snap.rtc.second);
  }
  off = wifi_json_append_cstr(s_wifiSettingsJsonBuf, sizeof(s_wifiSettingsJsonBuf), off, "}}");
  if (!wifi_json_valid(off, sizeof(s_wifiSettingsJsonBuf))) {
    return wifi_json_overflow_response();
  }
  return s_wifiSettingsJsonBuf;
}

static const char *wifi_getWifiJson() {
  char ssid[16];
  char pass[WIFI_AP_PASS_LEN + 1];
  const bool hasCreds = wifi_manager_getApCredentials(ssid, sizeof(ssid), pass, sizeof(pass));
  const bool active = wifi_manager_isActive();

  int off = wifi_json_off(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), 0, "{\"active\":%s,\"ssid\":\"",
                          active ? "true" : "false");
  if (hasCreds) {
    off = wifi_json_append_escape(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), off, ssid);
  }
  off = wifi_json_append_cstr(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), off, "\",\"ip\":\"");
  off = wifi_json_append_escape(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), off, wifi_manager_getApIp());
  off = wifi_json_append_cstr(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), off, "\",\"password_hidden\":true");
  if (active && hasCreds) {
    off = wifi_json_append_cstr(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), off, ",\"qr_url\":\"/api/wifi/qr.svg\"");
  }
  off = wifi_json_append_cstr(s_wifiInfoJsonBuf, sizeof(s_wifiInfoJsonBuf), off, "}");
  if (!wifi_json_valid(off, sizeof(s_wifiInfoJsonBuf))) {
    return wifi_json_overflow_response();
  }
  return s_wifiInfoJsonBuf;
}

static bool wifi_webGetPostParam(AsyncWebServerRequest *request, const char *name, char *out, size_t outLen) {
  if (out == nullptr || outLen == 0) {
    return false;
  }
  out[0] = '\0';
  if (request->hasParam(name, true)) {
    strncpy(out, request->getParam(name, true)->value().c_str(), outLen);
    out[outLen - 1] = '\0';
    return true;
  }
  if (request->hasParam(name, false)) {
    strncpy(out, request->getParam(name, false)->value().c_str(), outLen);
    out[outLen - 1] = '\0';
    return true;
  }
  return false;
}

static bool wifi_webMapField(const char *fieldName, int value, AppSetFieldPayload &payload) {
  if (fieldName == nullptr) {
    return false;
  }
  for (size_t i = 0; i < sizeof(kWifiFields) / sizeof(kWifiFields[0]); i++) {
    const WifiFieldDef &def = kWifiFields[i];
    if (strcmp(fieldName, def.name) != 0) {
      continue;
    }
    payload.field = def.field;
    payload.itemType = def.itemType;
    if (def.field == SF_WIFI_TIMEOUT) {
      static const uint8_t kWifiTmoChoices[WIFI_AP_SESSION_CHOICE_COUNT] = {
          WIFI_AP_SESSION_NEVER, 30, 60, 120, WIFI_AP_SESSION_MIN_MAX};
      bool allowed = false;
      for (uint8_t i = 0; i < WIFI_AP_SESSION_CHOICE_COUNT; i++) {
        if ((uint8_t)value == kWifiTmoChoices[i]) {
          allowed = true;
          break;
        }
      }
      if (!allowed) {
        return false;
      }
      payload.value = (int16_t)value;
    } else {
      payload.value = (int16_t)constrain(value, def.minv, def.maxv);
    }
    if (def.field == SF_LIGHT_BRIGHTNESS) {
      int v = (int)payload.value;
      if (v < 0) {
        v = 0;
      }
      if (v > 100) {
        v = 100;
      }
      v = ((v + 5) / 10) * 10;
      payload.value = (int16_t)v;
    } else if (settings_fieldIsBrightnessPercent(def.field)) {
      const uint8_t step = (def.field == SF_RGB_BRIGHTNESS) ? 10U : 5U;
      int v = (int)payload.value;
      if (v < 0) {
        v = 0;
      }
      if (v > 100) {
        v = 100;
      }
      v = ((v + (int)step / 2) / (int)step) * (int)step;
      payload.value = (int16_t)v;
    }
    if (def.itemType == IT_BOOL) {
      payload.value = (payload.value != 0) ? 1 : 0;
    }
    return true;
  }
  return false;
}

static void wifi_webHandleCommand(AsyncWebServerRequest *request) {
  if (!wifi_webRequireAuth(request)) {
    return;
  }
  char cmd[32];
  if (wifi_webGetPostParam(request, "cmd", cmd, sizeof(cmd))) {
    if (strcmp(cmd, "light_override") == 0) {
      const bool ok = app_dispatchApply(CMD_TOGGLE_LIGHT_OVERRIDE, nullptr);
      request->send(200, "application/json", ok ? F("{\"ok\":true}") : F("{\"ok\":false,\"error\":\"dispatch\"}"));
      return;
    }
    if (strcmp(cmd, "reset_settings") == 0) {
      const bool ok = app_dispatchApply(CMD_RESET_SETTINGS, nullptr);
      request->send(200, "application/json", ok ? F("{\"ok\":true}") : F("{\"ok\":false,\"error\":\"dispatch\"}"));
      return;
    }
#if SENSOR_LOG_ENABLE
    if (strcmp(cmd, "clear_sensor_log") == 0) {
      const bool ok = app_dispatchApply(CMD_CLEAR_SENSOR_LOG, nullptr);
      request->send(200, "application/json", ok ? F("{\"ok\":true}") : F("{\"ok\":false,\"error\":\"clear\"}"));
      return;
    }
#endif
    if (strcmp(cmd, "set_rtc") == 0) {
      AppSetRtcPayload rtcPayload = {2000, 1, 1, 0, 0, 0};
      char part[16];
      if (wifi_webGetPostParam(request, "year", part, sizeof(part))) {
        rtcPayload.year = (uint16_t)constrain(atoi(part), 2000, 2099);
      }
      if (wifi_webGetPostParam(request, "month", part, sizeof(part))) {
        rtcPayload.month = (uint8_t)constrain(atoi(part), 1, 12);
      }
      if (wifi_webGetPostParam(request, "day", part, sizeof(part))) {
        rtcPayload.day = (uint8_t)constrain(atoi(part), 1, 31);
      }
      if (wifi_webGetPostParam(request, "hour", part, sizeof(part))) {
        rtcPayload.hour = (uint8_t)constrain(atoi(part), 0, 23);
      }
      if (wifi_webGetPostParam(request, "minute", part, sizeof(part))) {
        rtcPayload.minute = (uint8_t)constrain(atoi(part), 0, 59);
      }
      if (wifi_webGetPostParam(request, "second", part, sizeof(part))) {
        rtcPayload.second = (uint8_t)constrain(atoi(part), 0, 59);
      }
      const bool ok = app_dispatchApply(CMD_SET_RTC, &rtcPayload);
      request->send(200, "application/json", ok ? F("{\"ok\":true}") : F("{\"ok\":false,\"error\":\"rtc\"}"));
      return;
    }
    request->send(400, "application/json", F("{\"ok\":false,\"error\":\"unknown cmd\"}"));
    return;
  }

  char field[32];
  char valueStr[16];
  if (!wifi_webGetPostParam(request, "field", field, sizeof(field)) ||
      !wifi_webGetPostParam(request, "value", valueStr, sizeof(valueStr))) {
    request->send(400, "application/json", F("{\"ok\":false,\"error\":\"field/value\"}"));
    return;
  }

  AppSetFieldPayload payload = {SF_NONE, IT_INT, 0};
  const int value = atoi(valueStr);
  if (!wifi_webMapField(field, value, payload)) {
    request->send(400, "application/json", F("{\"ok\":false,\"error\":\"unknown field\"}"));
    return;
  }

  const bool ok = app_dispatchApply(CMD_SET_FIELD, &payload);
  request->send(200, "application/json", ok ? F("{\"ok\":true}") : F("{\"ok\":false,\"error\":\"dispatch\"}"));
}

static void wifi_webServeQrSvg(AsyncWebServerRequest *request) {
  if (!wifi_webRequireAuth(request)) {
    return;
  }
  if (!wifi_manager_isActive()) {
    request->send(404, "text/plain", "SoftAP inactive");
    return;
  }
  char payload[80];
  if (!wifi_manager_buildJoinPayload(payload, sizeof(payload))) {
    request->send(500, "text/plain", "QR payload");
    return;
  }
  int svgLen = 0;
  if (!wifi_qr_buildSvg(payload, s_wifiQrSvgBuf, sizeof(s_wifiQrSvgBuf), 4, &svgLen)) {
    request->send(500, "text/plain", "QR overflow");
    return;
  }
  request->send(200, "image/svg+xml", s_wifiQrSvgBuf);
}

void wifi_web_registerRoutes(AsyncWebServer *server) {
  if (server == nullptr) {
    return;
  }
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifi_sendCompositePage(request, WIFI_PAGE_DASH_HTML, WIFI_PAGE_DASH_HTML2, WIFI_WEB_CSS, wifi_connJs(),
                           WIFI_PAGE_DASH_JS, nullptr);
  });
  server->on("/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "image/svg+xml", FPSTR(WEB_LOGO_SVG));
  });
  server->on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/dev/settings");
  });
  server->on("/dev/heat", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/hum", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/fan", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/light", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/inc", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/settings", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/clock", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/display", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/rgb", HTTP_GET, wifi_webSendDevPage);
  server->on("/dev/wifi", HTTP_GET, wifi_webSendDevPage);
#if SENSOR_LOG_ENABLE
  server->on("/dev/log", HTTP_GET, wifi_webSendDevPage);
  server->on("/hist/inc", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifi_sendCompositePage(request, WIFI_PAGE_HIST_HTML, WIFI_PAGE_HIST_BODY, WIFI_WEB_CSS, wifi_connJs(),
                           WIFI_PAGE_HIST_JS, nullptr);
  });
  server->on("/hist/grh_t", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifi_sendCompositePage(request, WIFI_PAGE_HIST_HTML, WIFI_PAGE_HIST_BODY, WIFI_WEB_CSS, wifi_connJs(),
                           WIFI_PAGE_HIST_JS, nullptr);
  });
  server->on("/hist/grh_h", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifi_sendCompositePage(request, WIFI_PAGE_HIST_HTML, WIFI_PAGE_HIST_BODY, WIFI_WEB_CSS, wifi_connJs(),
                           WIFI_PAGE_HIST_JS, nullptr);
  });
  server->on("/api/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    request->send(200, "application/json", sensor_log_getConfigJson());
  });
  server->on("/api/log", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    AppSetLogPayload payload = {};
    char enabledStr[8];
    char intervalStr[8];
    if (wifi_webGetPostParam(request, "enabled", enabledStr, sizeof(enabledStr))) {
      payload.hasEnabled = true;
      payload.enabled = atoi(enabledStr) != 0;
    }
    if (wifi_webGetPostParam(request, "interval_min", intervalStr, sizeof(intervalStr))) {
      payload.hasInterval = true;
      payload.intervalMin = (uint8_t)atoi(intervalStr);
    }
    const bool ok = app_dispatchApply(CMD_SET_LOG, &payload);
    if (ok) {
      request->send(200, "application/json", sensor_log_getConfigJson());
    } else {
      request->send(500, "application/json", F("{\"ok\":false,\"error\":\"dispatch\"}"));
    }
  });
  server->on("/api/history", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    if (!request->hasParam("series")) {
      request->send(400, "application/json", F("{\"ok\":false,\"error\":\"series\"}"));
      return;
    }
    const char *seriesStr = request->getParam("series")->value().c_str();
    SensorLogSeries series;
    if (!sensor_log_parseSeries(seriesStr, series)) {
      request->send(400, "application/json", F("{\"ok\":false,\"error\":\"series\"}"));
      return;
    }
    uint32_t hours = 24;
    if (request->hasParam("hours")) {
      hours = (uint32_t)atoi(request->getParam("hours")->value().c_str());
    }
    uint16_t points = 300;
    if (request->hasParam("points")) {
      points = (uint16_t)atoi(request->getParam("points")->value().c_str());
    }
    request->send(200, "application/json", sensor_log_getHistoryJson(series, hours, points));
  });
#else
  server->on("/dev/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/dev/settings");
  });
#endif
  server->on("/dev/system", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/dev/settings");
  });
  server->on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request) {
#if WIFI_WEB_AUTH_ENABLE
    char passStr[WIFI_AP_PASS_LEN + 1];
    if (!wifi_webGetPostParam(request, "password", passStr, sizeof(passStr))) {
      request->send(400, "application/json", F("{\"ok\":false,\"error\":\"password\"}"));
      return;
    }
    if (!wifi_webValidateApPassword(passStr)) {
      request->send(401, "application/json", F("{\"ok\":false,\"error\":\"auth\"}"));
      return;
    }
    wifi_webIssueSession();
    int off = wifi_json_off(s_wifiAuthJsonBuf, sizeof(s_wifiAuthJsonBuf), 0, "{\"ok\":true,\"token\":\"");
    off = wifi_json_append_escape(s_wifiAuthJsonBuf, sizeof(s_wifiAuthJsonBuf), off, s_webSessionToken);
    off = wifi_json_append_cstr(s_wifiAuthJsonBuf, sizeof(s_wifiAuthJsonBuf), off, "\"}");
    if (!wifi_json_valid(off, sizeof(s_wifiAuthJsonBuf))) {
      request->send(500, "application/json", wifi_json_overflow_response());
      return;
    }
    request->send(200, "application/json", s_wifiAuthJsonBuf);
#else
    request->send(200, "application/json", F("{\"ok\":true,\"token\":\"off\"}"));
#endif
  });
  server->on("/sys", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifi_sendCompositePage(request, WIFI_PAGE_SYS_HTML, WIFI_PAGE_SYS_BODY, WIFI_WEB_CSS, wifi_connJs(),
                           WIFI_PAGE_SYS_JS, nullptr);
  });
  server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    request->send(200, "application/json", wifi_getStatusJson());
  });
  server->on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    request->send(200, "application/json", wifi_getSettingsJson());
  });
  server->on("/api/wifi/qr.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    wifi_webServeQrSvg(request);
  });
  server->on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    request->send(200, "application/json", wifi_getWifiJson());
  });
  server->on("/api/sys", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_webRequireAuth(request)) {
      return;
    }
    request->send(200, "application/json", wifi_getSysJson());
  });
  server->on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request) {
    wifi_webHandleCommand(request);
  });
}

#endif
