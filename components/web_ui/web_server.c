#include "web_server.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

#define WIFI_SSID "LED_SYSTEM"
#define WIFI_PASS "12345678"

// Стан системи
static int t_r=0, t_g=0, t_b=0, t_w=0;
static int manual_br = 100;
static int mode = 0;
static int speed = 50;
static bool auto_mode = false;
static float display_lux = 0.0;

const char* html_page = "<!DOCTYPE html><html lang='uk'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Wadzik Disco</title>"
"<style>:root{--bg-dark:#0f0c29;--bg-gradient:linear-gradient(135deg,#0f0c29,#302b63,#24243e);--glass-bg:rgba(255,255,255,0.05);--glass-border:rgba(255,255,255,0.1);--text-main:#ffffff;--text-secondary:#b3b3b3;--active-color:#00d2ff}*{box-sizing:border-box;margin:0;padding:0}body{font-family:sans-serif;background:var(--bg-gradient);color:var(--text-main);min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}body::before{content:'';position:absolute;width:300px;height:300px;background:var(--active-color);filter:blur(150px);border-radius:50%;opacity:0.15;z-index:-1}.controller{background:var(--glass-bg);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);border:1px solid var(--glass-border);width:100%;max-width:400px;padding:30px;border-radius:30px;text-align:center}.hex-display{font-size:1.5rem;letter-spacing:2px;margin:20px 0;text-shadow:0 0 10px var(--active-color)}.color-circle-wrapper{position:relative;width:160px;height:160px;margin:0 auto;border-radius:50%;border:4px solid rgba(255,255,255,.05);display:flex;justify-content:center;align-items:center;box-shadow:0 0 30px var(--active-color)}input[type=color]{width:150px;height:150px;border:none;border-radius:50%;cursor:pointer;opacity:0;position:absolute}.rainbow-palette{display:flex;justify-content:space-between;margin:20px 0;background:rgba(0,0,0,.2);padding:10px;border-radius:20px}.color-dot{width:30px;height:30px;border-radius:50%;cursor:pointer}.slider-container{margin:20px 0;text-align:left;background:rgba(0,0,0,0.2);padding:15px;border-radius:15px}input[type=range]{width:100%;height:6px;background:rgba(255,255,255,.1);border-radius:5px;-webkit-appearance:none}.label-group{display:flex;justify-content:space-between;margin-bottom:10px;color:#ccc}.modes-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px}.mode-btn{padding:15px;background:rgba(255,255,255,0.1);border:1px solid rgba(255,255,255,0.2);color:white;border-radius:10px;cursor:pointer;text-transform:uppercase;font-weight:bold}.mode-btn:active{background:var(--active-color);color:black}"
".switch{position:relative;display:inline-block;width:50px;height:24px}.switch input{opacity:0;width:0;height:0}.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:34px}.slider:before{position:absolute;content:'';height:16px;width:16px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%}input:checked+.slider{background-color:#2196F3}input:checked+.slider:before{transform:translateX(26px)}.auto-row{display:flex;justify-content:space-between;align-items:center;background:rgba(0,0,0,0.3);padding:15px;border-radius:15px;margin:15px 0}</style>"
"</head><body><div class='controller'><h2> SUPER COOL SYSTEM</h2>"
"<div class='hex-display' id='hexDisplay'>#00D2FF</div>"
"<div class='color-circle-wrapper' style='background: #00d2ff' id='bgRing'><input type='color' id='colorPicker' value='#00d2ff'></div>"
"<div class='rainbow-palette'><div class='color-dot' style='background:#ff0000' onclick=\"setColor('#ff0000')\"></div><div class='color-dot' style='background:#00ff00' onclick=\"setColor('#00ff00')\"></div><div class='color-dot' style='background:#0000ff' onclick=\"setColor('#0000ff')\"></div><div class='color-dot' style='background:#ffffff' onclick=\"setColor('#ffffff'); setWhite(100);\"></div></div>"

"<div class='auto-row'><span><b>AUTO MODE</b> (Lux Sensor)</span><label class='switch'><input type='checkbox' id='autoCheck' onclick='toggleAuto()'><span class='slider'></span></label></div>"
"<div class='slider-container'><div class='label-group'><span>Brightness</span><span id='brVal'>100%</span></div><input type='range' id='brightness' min='0' max='100' value='100'></div>"
"<div class='slider-container'><div class='label-group'><span>White (W)</span><span id='wVal'>0%</span></div><input type='range' id='whiteCh' min='0' max='100' value='0'></div>"
"<div class='slider-container'><div class='label-group'><span>Speed</span><span id='spVal'>50%</span></div><input type='range' id='speed' min='1' max='100' value='50'></div>"
"<div class='modes-grid'><button class='mode-btn' onclick='setMode(0)'>Static</button><button class='mode-btn' onclick='setMode(1)'>Rainbow</button><button class='mode-btn' onclick='setMode(2)'>Strobe</button><button class='mode-btn' onclick='setMode(3)'>Breath</button></div>"
"<p style='color:gray;font-size:0.8rem;margin-top:10px' id='luxVal'>Lux: --</p>"
"</div><script>"
"const picker=document.getElementById('colorPicker');const hexDisp=document.getElementById('hexDisplay');"
"const brSlider=document.getElementById('brightness');const wSlider=document.getElementById('whiteCh');const spSlider=document.getElementById('speed');const ac=document.getElementById('autoCheck');"
"const bgRing=document.getElementById('bgRing');const root=document.documentElement;"
"let lastSend=0;"
"function send(){"
"  const now=Date.now(); if(now-lastSend<50)return; lastSend=now;"
"  const hex=picker.value; const br=brSlider.value; const wVal=wSlider.value;"
"  const r=parseInt(hex.slice(1,3),16);const g=parseInt(hex.slice(3,5),16);const b=parseInt(hex.slice(5,7),16);"
"  var xhr=new XMLHttpRequest(); xhr.open('GET','/set?r='+r+'&g='+g+'&b='+b+'&br='+br+'&w='+wVal,true); xhr.send();"
"}"
"function updateUI(hex){ root.style.setProperty('--active-color',hex); hexDisp.innerText=hex.toUpperCase(); bgRing.style.background=hex; }"
"picker.addEventListener('input',(e)=>{ updateUI(e.target.value); send(); });"
"brSlider.addEventListener('input',(e)=>{ document.getElementById('brVal').innerText=e.target.value+'%'; send(); });"
"wSlider.addEventListener('input',(e)=>{ document.getElementById('wVal').innerText=e.target.value+'%'; send(); });"
"spSlider.addEventListener('input',(e)=>{ document.getElementById('spVal').innerText=e.target.value+'%'; var xhr=new XMLHttpRequest(); xhr.open('GET','/speed?val='+e.target.value,true); xhr.send(); });"
"function setColor(hex){ picker.value=hex; updateUI(hex); wSlider.value=0; document.getElementById('wVal').innerText='0%'; send(); }"
"function setWhite(val){ wSlider.value=val; document.getElementById('wVal').innerText=val+'%'; send(); }"
"function setMode(m){ var xhr=new XMLHttpRequest(); xhr.open('GET','/mode?val='+m,true); xhr.send(); }"
"function toggleAuto(){ var xhr=new XMLHttpRequest(); xhr.open('GET','/auto?val='+(ac.checked?1:0),true); xhr.send(); }"

"setInterval(()=>{ fetch('/status').then(r=>r.json()).then(d=>{ document.getElementById('luxVal').innerText='Lux: '+d.lux.toFixed(1); if(d.auto && !ac.checked) ac.checked=true; }); }, 2000);"
"</script></body></html>"; 


// --- HANDLERS ---
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
    mode = 0; // Скидання в статику
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

// --- INIT ---
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
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/",.method=HTTP_GET,.handler=root_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/set",.method=HTTP_GET,.handler=set_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/auto",.method=HTTP_GET,.handler=auto_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/status",.method=HTTP_GET,.handler=status_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/mode",.method=HTTP_GET,.handler=mode_h});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/speed",.method=HTTP_GET,.handler=speed_h});
    }
}

// --- GETTERS ---
void web_get_target_color(int *r, int *g, int *b, int *w) { *r=t_r; *g=t_g; *b=t_b; *w=t_w; }
int web_get_brightness(void) { return manual_br; }
int web_get_mode(void) { return mode; }
int web_get_speed(void) { return speed; }
bool web_is_auto_mode(void) { return auto_mode; }
void web_update_lux_display(float lux) { display_lux = lux; }