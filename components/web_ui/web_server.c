#include "web_server.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>
#include "ds3231.h"

// --- DNS ТА MDNS ---
#include <sys/param.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "mdns.h" 

#define WIFI_SSID "PadWadYak3000"
#define WIFI_PASS "12345678"

#define I2C_MASTER_NUM       I2C_NUM_1 

// Стан системи
static int t_r=0, t_g=0, t_b=0, t_w=0;
static int manual_br = 100;
static int mode = 0;
static int speed = 50;
static bool auto_mode = false;
static float display_lux = 0.0;

const char* html_page = "<!DOCTYPE html><html lang=\"uk\"><head><meta charset=\"utf-8\"/><meta name=\"viewport\" content=\"width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no\"/><title>PadWadYak3000 clock</title><style>:root{--bg:#1a0b2e;--grad:linear-gradient(135deg,#0f0518,#1f0b3d,#12061f);--glass:rgba(255,255,255,0.05);--glass-border:rgba(255,255,255,0.1);--accent:#00d2ff;--text:#fff;--danger:#ff4444;--slot-height:40px}*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;scrollbar-width:none;user-select:none}*::-webkit-scrollbar{display:none}body{margin:0;background:var(--grad);color:var(--text);font-family:system-ui,-apple-system,sans-serif;height:100vh;overflow:hidden;display:flex;flex-direction:column}.app{width:100%;max-width:420px;margin:0 auto;height:100%;display:flex;flex-direction:column;padding:20px;position:relative}.header{padding:15px 0;text-align:center;margin-bottom:5px}.title{font-family:monospace;font-weight:700;font-size:1.2rem;letter-spacing:2px;color:rgba(255,255,255,0.9);text-shadow:0 0 10px rgba(0,210,255,0.3)}.view{display:none;flex:1;flex-direction:column;align-items:center;width:100%;overflow-y:auto;padding-bottom:100px}.view.active{display:flex}.card{width:100%;background:rgba(255,255,255,0.03);backdrop-filter:blur(10px);border-radius:24px;padding:20px;border:1px solid var(--glass-border);display:flex;flex-direction:column;align-items:center}.clock-face{width:200px;height:200px;border-radius:50%;background:radial-gradient(circle at center,transparent 50%,rgba(0,0,0,0.4));border:3px solid rgba(255,255,255,0.05);position:relative;margin-bottom:10px;box-shadow:inset 0 0 20px rgba(0,0,0,0.5)}.hand{position:absolute;bottom:50%;left:50%;transform-origin:bottom center;border-radius:4px;transform:translateX(-50%) rotate(0deg)}.hour{width:6px;height:50px;background:#fff;z-index:2}.minute{width:4px;height:75px;background:var(--accent);z-index:3}.second{width:2px;height:85px;background:var(--danger);z-index:4}.center-dot{position:absolute;top:50%;left:50%;width:8px;height:8px;border-radius:50%;background:#fff;transform:translate(-50%,-50%);z-index:5}.tick{position:absolute;width:2px;height:10px;background:rgba(255,255,255,0.3);left:50%;transform-origin:center 100px;top:0}.tick.major{width:4px;height:15px;background:#fff}.picker-container{display:flex;justify-content:center;align-items:center;gap:10px;margin:10px 0;padding:10px;background:rgba(0,0,0,0.2);border-radius:16px;border:1px solid var(--glass-border);width:100%}.picker-col{display:flex;flex-direction:column;align-items:center;flex:1}.wheel-window{height:120px;width:100%;min-width:50px;overflow-y:scroll;overflow-x:hidden;scroll-snap-type:y mandatory;position:relative;background:rgba(0,0,0,0.3);border-radius:8px;border:1px solid rgba(255,255,255,0.05);mask-image:linear-gradient(to bottom,transparent,black 20%,black 80%,transparent);-webkit-mask-image:linear-gradient(to bottom,transparent,black 20%,black 80%,transparent)}.wheel-item{height:var(--slot-height);display:flex;align-items:center;justify-content:center;font-family:monospace;font-weight:700;font-size:1.2rem;color:rgba(255,255,255,0.3);scroll-snap-align:center}.wheel-item.active{color:#fff;text-shadow:0 0 10px var(--accent);transform:scale(1.1)}.highlight-bar{position:absolute;top:50%;left:0;width:100%;height:var(--slot-height);transform:translateY(-50%);border-top:1px solid var(--accent);border-bottom:1px solid var(--accent);background:rgba(0,210,255,0.05);pointer-events:none;z-index:10}.picker-arrow{background:transparent;border:none;color:var(--accent);font-size:1rem;cursor:pointer;padding:8px;opacity:0.7;transition:0.1s;display:flex;align-items:center;justify-content:center}.picker-arrow:active{transform:scale(1.2);color:#fff;opacity:1}.picker-sep{font-family:monospace;font-weight:700;font-size:1.2rem;color:#666;padding-top:0}.section-label{font-family:monospace;font-weight:700;font-size:0.7rem;color:#666;width:100%;text-align:center;margin-top:5px;letter-spacing:1px}.set-btn{width:100%;margin-top:15px;padding:12px;background:var(--accent);border-radius:12px;border:none;color:#000;font-family:monospace;font-weight:700;font-size:1rem;cursor:pointer;box-shadow:0 0 15px rgba(0,210,255,0.2);transition:0.2s}.set-btn:active{transform:scale(0.98);opacity:0.8}.sync-btn{width:100%;margin-top:15px;padding:12px;background:transparent;border:1px solid var(--glass-border);border-radius:12px;color:#888;font-family:monospace;font-size:0.8rem;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px}.sync-btn:active{background:rgba(255,255,255,0.1);color:#fff;border-color:#fff}.switch-row{width:100%;display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;font-size:0.9rem;color:#aaa}.switch-row input[type='checkbox']{display:none}.ios-switch{position:relative;width:44px;height:24px;background:#333;border-radius:20px;transition:0.3s}.ios-switch::after{content:'';position:absolute;top:2px;left:2px;width:20px;height:20px;background:#fff;border-radius:50%;transition:0.3s}input:checked+.ios-switch{background:var(--accent)}input:checked+.ios-switch::after{transform:translateX(20px)}.color-ring{width:180px;height:180px;border-radius:50%;border:3px solid rgba(255,255,255,0.1);display:flex;align-items:center;justify-content:center;margin:10px 0;position:relative}input[type='color']{position:absolute;width:100%;height:100%;opacity:0;cursor:pointer}.slider-box{width:100%;margin-top:15px}.slider-label{display:flex;justify-content:space-between;font-size:0.8rem;color:#888;margin-bottom:5px}input[type='range']{width:100%;height:6px;background:#333;border-radius:10px;-webkit-appearance:none;outline:none}input[type='range']::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;background:#fff;border-radius:50%;box-shadow:0 0 10px var(--accent)}.modes-box{display:grid;grid-template-columns:1fr 1fr;gap:8px;width:100%;margin-top:20px}.mode-tag{background:rgba(255,255,255,0.05);padding:12px;text-align:center;border-radius:8px;font-size:0.75rem;font-weight:bold;color:#aaa;cursor:pointer;border:1px solid transparent}.mode-tag.active{background:var(--accent);color:#000;border-color:var(--accent)}.nav{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#150a25;border:1px solid var(--glass-border);border-radius:40px;padding:5px 20px;display:flex;gap:20px;box-shadow:0 10px 30px rgba(0,0,0,0.5);z-index:100}.nav-btn{width:50px;height:50px;border-radius:50%;color:rgba(255,255,255,0.4);font-size:1.2rem;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:0.2s}.nav-btn svg{width:24px;height:24px;fill:currentColor}.nav-btn.active{background:var(--accent);color:#000;box-shadow:0 0 15px var(--accent-glow)}a{color:var(--accent);text-decoration:none;font-size:0.7rem;display:block;margin-top:5px;font-family:monospace}</style></head><body><div class='app'><div class='header'><div class='title' title='MEGAKNIGHT SYSTEM MEGACOOL CONTROL EVOLUTION'>PadWadYak3000 configuration</div><a class='section-label' target='_blank' href='http://PadWadYak3000'>For next connection use this link: http://PadWadYak3000</a></div><div id='v-clock' class='view active'><div class='card'><div class='clock-face' id='clockFace'><div class='center-dot'></div><div class='hand hour' id='hh'></div><div class='hand minute' id='mh'></div><div class='hand second' id='sh'></div></div><div style='font-family:monospace;font-weight:700;font-size:2.5rem' id='digTime'>00:00</div><div style='color:#888;font-size:0.9rem;margin-bottom:5px' id='dateStr'>--</div><div class='section-label'>TIME CONTROL</div><div class='picker-container'><div class='picker-col'><button class='picker-arrow' onclick=\"moveWheel('wh-h',-1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z'/></svg></button><div style='position:relative;width:100%'><div class='highlight-bar'></div><div class='wheel-window' id='wh-h' data-min='0'></div></div><button class='picker-arrow' onclick=\"moveWheel('wh-h',1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z'/></svg></button></div><div class='picker-sep'>:</div><div class='picker-col'><button class='picker-arrow' onclick=\"moveWheel('wh-m',-1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z'/></svg></button><div style='position:relative;width:100%'><div class='highlight-bar'></div><div class='wheel-window' id='wh-m' data-min='0'></div></div><button class='picker-arrow' onclick=\"moveWheel('wh-m',1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z'/></svg></button></div></div><div class='section-label'>DATE CONTROL</div><div class='picker-container'><div class='picker-col'><button class='picker-arrow' onclick=\"moveWheel('wh-d',-1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z'/></svg></button><div style='position:relative;width:100%'><div class='highlight-bar'></div><div class='wheel-window' id='wh-d' data-min='1'></div></div><button class='picker-arrow' onclick=\"moveWheel('wh-d',1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z'/></svg></button></div><div class='picker-sep'>.</div><div class='picker-col'><button class='picker-arrow' onclick=\"moveWheel('wh-mo',-1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z'/></svg></button><div style='position:relative;width:100%'><div class='highlight-bar'></div><div class='wheel-window' id='wh-mo' data-min='1'></div></div><button class='picker-arrow' onclick=\"moveWheel('wh-mo',1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z'/></svg></button></div><div class='picker-sep'>.</div><div class='picker-col'><button class='picker-arrow' onclick=\"moveWheel('wh-y',-1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z'/></svg></button><div style='position:relative;width:100%'><div class='highlight-bar'></div><div class='wheel-window' id='wh-y' data-min='2024'></div></div><button class='picker-arrow' onclick=\"moveWheel('wh-y',1)\"><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor'><path d='M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z'/></svg></button></div></div><button class='set-btn' onclick='setSysTime()'>SET TIME & DATE</button><button class='sync-btn' onclick='syncBrowser()'><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor' style='margin-right:5px'><path d='M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z'/></svg> Synchronize with gadget</button></div></div><div id='v-led' class='view'><div class='card'><div class='switch-row'><span><svg viewBox='0 0 24 24' width='16' height='16' fill='currentColor' style='vertical-align:middle;margin-right:5px'><path d='M12 2a1 1 0 0 1 1 1v2a1 1 0 1 1-2 0V3a1 1 0 0 1 1-1zm5.66 2.93a1 1 0 0 1 1.41 1.41l-1.42 1.42a1 1 0 1 1-1.41-1.42l1.42-1.41zM12 18a1 1 0 0 1 1 1v2a1 1 0 1 1-2 0v-2a1 1 0 0 1 1-1zm-5.66 2.93a1 1 0 0 1-1.41-1.41l1.42-1.42a1 1 0 1 1 1.41 1.42l-1.42 1.41zM2 12a1 1 0 0 1 1-1h2a1 1 0 1 1 0 2H3a1 1 0 0 1-1-1zm2.93 5.66a1 1 0 0 1-1.41-1.41l1.42-1.42a1 1 0 1 1 1.41 1.42l-1.42 1.41zM22 12a1 1 0 0 1-1 1h-2a1 1 0 1 1 0-2h2a1 1 0 0 1 1 1zm-2.93-5.66a1 1 0 0 1 1.41-1.41l-1.42 1.42a1 1 0 1 1-1.41-1.42l1.42-1.41zM12 7a5 5 0 1 0 0 10 5 5 0 0 0 0-10z'/></svg> Auto-brightness</span><label><input type='checkbox' id='autoCheck' onclick='togAuto()'/><div class='ios-switch'></div></label></div><div style='font-size:0.8rem;color:#666;width:100%;text-align:right;margin-bottom:10px' id='luxVal'>Lux: --</div><div class='color-ring' id='ring'><svg viewBox='0 0 24 24' width='48' height='48' fill='currentColor' style='opacity:0.5'><path d='M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7z'/></svg><input type='color' id='cp' value='#00d2ff'/></div><div style='font-family:monospace;font-weight:700;margin-bottom:15px;font-size:1.2rem' id='hexText'>#00D2FF</div><div class='slider-box'><div class='slider-label'><span>Brightness</span><span id='brV'>100%</span></div><input type='range' id='br' min='0' max='100' value='100'/></div><div class='slider-box'><div class='slider-label'><span>White</span><span id='wV'>0%</span></div><input type='range' id='w' min='0' max='100' value='0'/></div><div class='slider-box'><div class='slider-label'><span>Speed</span><span id='spV'>50%</span></div><input type='range' id='sp' min='1' max='100' value='50'/></div><div class='modes-box'><div class='mode-tag active' onclick='sMode(0,this)'>STATIC</div><div class='mode-tag' onclick='sMode(1,this)'>RAINBOW</div><div class='mode-tag' onclick='sMode(2,this)'>STROBE</div><div class='mode-tag' onclick='sMode(3,this)'>BREATH</div></div></div></div></div><div class='nav'><div class='nav-btn active' onclick=\"nav('v-clock',this)\"><svg viewBox='0 0 24 24'><path d='M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67z'/></svg></div><div class='nav-btn' onclick=\"nav('v-led',this)\"><svg viewBox='0 0 24 24'><path d='M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7z'/></svg></div></div><script>const face=document.getElementById('clockFace');for(let i=0;i<12;i++){let tk=document.createElement('div');tk.className='tick '+(i%3==0?'major':'');tk.style.transform=`translateX(-50%) rotate(${i*30}deg)`;face.appendChild(tk)}let timeOffset=0;function updateClock(){const d=new Date(Date.now()+timeOffset);const h=d.getHours(),m=d.getMinutes(),s=d.getSeconds();document.getElementById('sh').style.transform=`translateX(-50%) rotate(${s*6}deg)`;document.getElementById('mh').style.transform=`translateX(-50%) rotate(${m*6+s*0.1}deg)`;document.getElementById('hh').style.transform=`translateX(-50%) rotate(${(h%12)*30+m*0.5}deg)`;document.getElementById('digTime').innerText=`${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}`;document.getElementById('dateStr').innerText=d.toLocaleDateString('uk-UA',{weekday:'long',day:'numeric',month:'long'})}setInterval(updateClock,1000);updateClock();let ls=0;function send(u){let n=Date.now();if(n-ls<50)return;ls=n;fetch(u).catch(()=>{})}function mkWheel(id,min,max){const el=document.getElementById(id);el.innerHTML='';el.dataset.min=min;let pt=document.createElement('div');pt.style.height='40px';el.appendChild(pt);for(let i=min;i<=max;i++){let d=document.createElement('div');d.className='wheel-item';d.innerText=i.toString().padStart(2,'0');el.appendChild(d)}let pb=document.createElement('div');pb.style.height='40px';el.appendChild(pb);el.addEventListener('scroll',()=>{let idx=Math.round(el.scrollTop/40);el.querySelectorAll('.wheel-item').forEach((it,i)=>{it.classList.toggle('active',i===idx)})})}function moveWheel(id,dir){const el=document.getElementById(id);el.scrollBy({top:dir*40,behavior:'smooth'})}function getWheelVal(id){const min=parseInt(document.getElementById(id).dataset.min);const idx=Math.round(document.getElementById(id).scrollTop/40);return idx+min}function setWheelVal(id,val){const el=document.getElementById(id);const min=parseInt(el.dataset.min);let targetIdx=val-min;if(targetIdx<0)targetIdx=0;el.scrollTop=targetIdx*40}mkWheel('wh-h',0,23);mkWheel('wh-m',0,59);mkWheel('wh-d',1,31);mkWheel('wh-mo',1,12);mkWheel('wh-y',2024,2040);function setSysTime(){let h=getWheelVal('wh-h');let m=getWheelVal('wh-m');let d=getWheelVal('wh-d');let mo=getWheelVal('wh-mo');let y=getWheelVal('wh-y');const now=new Date();const target=new Date();target.setFullYear(y);target.setMonth(mo-1);target.setDate(d);target.setHours(h,m,0);timeOffset=target.getTime()-now.getTime();updateClock();fetch(`/time?h=${h}&m=${m}&s=0`).catch(()=>{});setTimeout(()=>{fetch(`/date?d=${d}&m=${mo}&y=${y}`).catch(()=>{})},100)}function syncBrowser(){timeOffset=0;updateClock();const d=new Date();setWheelVal('wh-h',d.getHours());setWheelVal('wh-m',d.getMinutes());setWheelVal('wh-d',d.getDate());setWheelVal('wh-mo',d.getMonth()+1);setWheelVal('wh-y',d.getFullYear());fetch(`/time?h=${d.getHours()}&m=${d.getMinutes()}&s=${d.getSeconds()}`).catch(()=>{});setTimeout(()=>{fetch(`/date?d=${d.getDate()}&m=${d.getMonth()+1}&y=${d.getFullYear()}`).catch(()=>{})},100)}function upd(){let h=document.getElementById('cp').value;let br=document.getElementById('br').value;let w=document.getElementById('w').value;let r=parseInt(h.slice(1,3),16),g=parseInt(h.slice(3,5),16),b=parseInt(h.slice(5,7),16);document.getElementById('brV').innerText=br+'%';document.getElementById('wV').innerText=w+'%';document.getElementById('hexText').innerText=h.toUpperCase();document.getElementById('ring').style.boxShadow=`0 0 30px ${h}`;send(`/set?r=${r}&g=${g}&b=${b}&br=${br}&w=${w}`)}document.getElementById('cp').addEventListener('input',upd);document.getElementById('br').addEventListener('input',upd);document.getElementById('w').addEventListener('input',upd);document.getElementById('sp').addEventListener('input',(e)=>{document.getElementById('spV').innerText=e.target.value+'%';send(`/speed?val=${e.target.value}`)});function nav(id,el){document.querySelectorAll('.view').forEach((v)=>v.classList.remove('active'));document.getElementById(id).classList.add('active');document.querySelectorAll('.nav-btn').forEach((b)=>b.classList.remove('active'));el.classList.add('active');if(id==='v-clock'){setTimeout(()=>{document.querySelectorAll('.wheel-window').forEach((w)=>w.dispatchEvent(new Event('scroll')))},100)}}function sMode(m,el){document.querySelectorAll('.mode-tag').forEach((b)=>b.classList.remove('active'));el.classList.add('active');send(`/mode?val=${m}`)}function togAuto(){send('/auto?val='+(document.getElementById('autoCheck').checked?1:0))}setInterval(()=>{if(document.getElementById('v-led').classList.contains('active')){fetch('/status').then((r)=>r.json()).then((d)=>{document.getElementById('luxVal').innerText='Lux: '+d.lux.toFixed(1);if(d.auto&&!document.getElementById('autoCheck').checked)document.getElementById('autoCheck').checked=true}).catch(()=>{})}},2000);setTimeout(syncBrowser,500);</script></body></html>";
// HTTP HANDLERS
esp_err_t time_set_h(httpd_req_t *req) {
    char buf[100];
    char param[10];
    ds3231_time_t time;

    // Спочатку зчитуємо поточний час, щоб не збити дату
    esp_err_t err = ds3231_get_time(I2C_MASTER_NUM, &time);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Парсимо нові значення з URL
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if (httpd_query_key_value(buf, "h", param, sizeof(param)) == ESP_OK) {
            time.hours = atoi(param);
        }
        if (httpd_query_key_value(buf, "m", param, sizeof(param)) == ESP_OK) {
            time.minutes = atoi(param);
        }
        if (httpd_query_key_value(buf, "s", param, sizeof(param)) == ESP_OK) {
            time.seconds = atoi(param);
        }
        
        // Записуємо оновлену структуру назад у DS3231
        ds3231_set_time(I2C_MASTER_NUM, &time);
        ESP_LOGI("WEB", "Time updated: %02d:%02d:%02d", time.hours, time.minutes, time.seconds);
    }

    httpd_resp_send(req, "OK", -1);
    return ESP_OK;
}

esp_err_t date_set_h(httpd_req_t *req) {
    char buf[100];
    char param[10];
    ds3231_time_t time;

    // Зчитуємо поточний стан (щоб зберегти години/хвилини)
    esp_err_t err = ds3231_get_time(I2C_MASTER_NUM, &time);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Парсимо дату
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if (httpd_query_key_value(buf, "d", param, sizeof(param)) == ESP_OK) {
            time.day = atoi(param);
        }
        if (httpd_query_key_value(buf, "m", param, sizeof(param)) == ESP_OK) {
            time.month = atoi(param);
        }
        if (httpd_query_key_value(buf, "y", param, sizeof(param)) == ESP_OK) {
            time.year = atoi(param);
        }

        // Записуємо оновлену дату
        ds3231_set_time(I2C_MASTER_NUM, &time);
        ESP_LOGI("WEB", "Date updated: %02d.%02d.%04d", time.day, time.month, time.year);
    }

    httpd_resp_send(req, "OK", -1);
    return ESP_OK;
}

esp_err_t root_h(httpd_req_t *r) { httpd_resp_send(r, html_page, HTTPD_RESP_USE_STRLEN); return ESP_OK; }

esp_err_t set_h(httpd_req_t *req) {
    char buf[200]; char p[32];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if(httpd_query_key_value(buf, "r", p, 32)==0) t_r = atoi(p);
        if(httpd_query_key_value(buf, "g", p, 32)==0) t_g = atoi(p);
        if(httpd_query_key_value(buf, "b", p, 32)==0) t_b = atoi(p);
        if(httpd_query_key_value(buf, "w", p, 32)==0) t_w = atoi(p) * 255 / 100;
        if(httpd_query_key_value(buf, "br", p, 32)==0 && !auto_mode) manual_br = atoi(p);
    }
    mode = 0;
    httpd_resp_send(req, "OK", -1); return ESP_OK;
}

esp_err_t auto_h(httpd_req_t *req) {
    char buf[50]; char p[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if(httpd_query_key_value(buf, "val", p, 10)==0) auto_mode = atoi(p);
    }
    httpd_resp_send(req, "OK", -1); return ESP_OK;
}

esp_err_t status_h(httpd_req_t *req) {
    char resp[64]; sprintf(resp, "{\"lux\": %.1f, \"auto\": %d}", display_lux, auto_mode);
    httpd_resp_set_type(req, "application/json"); httpd_resp_send(req, resp, -1); return ESP_OK;
}

esp_err_t mode_h(httpd_req_t *req) {
    char buf[50]; char p[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if(httpd_query_key_value(buf, "val", p, 10)==0) mode = atoi(p);
    }
    httpd_resp_send(req, "OK", -1); return ESP_OK;
}

esp_err_t speed_h(httpd_req_t *req) {
    char buf[50]; char p[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if(httpd_query_key_value(buf, "val", p, 10)==0) speed = 105 - atoi(p);
    }
    httpd_resp_send(req, "OK", -1); return ESP_OK;
}

esp_err_t redirect_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirecting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// --- DNS SERVER TASK ---
void dns_server_task(void *pvParameters) {
    char rx_buffer[128];
    char tx_buffer[128];
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53);
    bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);
        if (len > 0) {
            tx_buffer[0] = rx_buffer[0]; tx_buffer[1] = rx_buffer[1];
            tx_buffer[2] = 0x81; tx_buffer[3] = 0x80;
            tx_buffer[4] = 0x00; tx_buffer[5] = 0x01;
            tx_buffer[6] = 0x00; tx_buffer[7] = 0x01;
            tx_buffer[8] = 0x00; tx_buffer[9] = 0x00;
            tx_buffer[10] = 0x00; tx_buffer[11] = 0x00;

            int query_len = 0;
            for (int i = 12; i < len; i++) {
                if (rx_buffer[i] == 0) { query_len = i - 12 + 1 + 4; break; }
            }
            if (query_len > 0) {
                memcpy(&tx_buffer[12], &rx_buffer[12], query_len);
                int ptr = 12 + query_len;
                tx_buffer[ptr++] = 0xC0; tx_buffer[ptr++] = 0x0C;
                tx_buffer[ptr++] = 0x00; tx_buffer[ptr++] = 0x01;
                tx_buffer[ptr++] = 0x00; tx_buffer[ptr++] = 0x01;
                tx_buffer[ptr++] = 0x00; tx_buffer[ptr++] = 0x00; 
                tx_buffer[ptr++] = 0x00; tx_buffer[ptr++] = 0x3C;
                tx_buffer[ptr++] = 0x00; tx_buffer[ptr++] = 0x04;
                tx_buffer[ptr++] = 192; tx_buffer[ptr++] = 168;
                tx_buffer[ptr++] = 4;   tx_buffer[ptr++] = 1;
                sendto(sock, tx_buffer, ptr, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// MDNS SERVICE
void start_mdns_service() {
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("PadWadYak3000"));
    ESP_ERROR_CHECK(mdns_instance_name_set("PadWadYak3000_network"));
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
}

// INIT
void wifi_init_softap() {
    esp_netif_init(); esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wc = { .ap = { .ssid=WIFI_SSID, .password=WIFI_PASS, .max_connection=4, .authmode=WIFI_AUTH_WPA_WPA2_PSK } };
    esp_wifi_set_mode(WIFI_MODE_AP); esp_wifi_set_config(WIFI_IF_AP, &wc); esp_wifi_start();
}

void web_init_system(void) {
    nvs_flash_init();
    wifi_init_softap();
    
    start_mdns_service();

    // Запуск DNS для Captive Portal
    xTaskCreate(dns_server_task, "dns_server", 2048, NULL, 5, NULL);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 15; 
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/",.method=HTTP_GET,.handler=root_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/set",.method=HTTP_GET,.handler=set_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/auto",.method=HTTP_GET,.handler=auto_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/status",.method=HTTP_GET,.handler=status_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/mode",.method=HTTP_GET,.handler=mode_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/speed",.method=HTTP_GET,.handler=speed_h});
		httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/time", .method=HTTP_GET, .handler=time_set_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/date", .method=HTTP_GET, .handler=date_set_h});
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect_handler);
    }
}

// GETTERS
void web_get_target_color(int *r, int *g, int *b, int *w) { *r=t_r; *g=t_g; *b=t_b; *w=t_w; }
int web_get_brightness(void) { return manual_br; }
int web_get_mode(void) { return mode; }
int web_get_speed(void) { return speed; }
bool web_is_auto_mode(void) { return auto_mode; }
void web_update_lux_display(float lux) { display_lux = lux; }