// =====================================================
// ESP32-C3 FULL CODE - WITH SIDE DROPDOWN & AUTO OFF LOGIC
// =====================================================

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

// রিলে টাইপ: Active High এর জন্য 0, Active Low এর জন্য 1
#define RELAY_ACTIVE_LOW 0

const char* ssid     = "HONER 200";
const char* password = "123456789";

WebServer server(80);

// ================= GPIO (ESP32-C3 SAFE PINOUT) =================
#define RELAY1 2
#define RELAY2 3
#define RELAY3 4
#define RELAY4 5
#define RELAY5 10    // রিলে ৫ এবং ৬ নিরাপদ জিপিআইও ১০ শেয়ার করছে
#define RELAY6 10    
#define AUTO_OFF_PIN 1 // অটো হেলমেট অফ মোডের ফিজিক্যাল আউটপুট পিন

#define LEDPIN 8          // Brightness PWM Pin
#define SIGNAL_PIN 7       // Signal Input Pin

#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// ================= STATES & MEMORY =================
bool relay1State=false;
bool relay2State=false;
bool relay3State=false;
bool relay4State=false;
bool relay5State=false;
bool relay6State=false;
bool autoOffState=false; // অটো হেলমেট অফ মোডের স্টেট ভ্যারিয়েবল

// Relay 6 এর ভেতরের সাব-বাটন দুটির স্টেট
bool r6Btn1State=false; 
bool r6Btn2State=false; 

bool signalActive=false;
bool waitingRestore=false;
bool lastSignal=false;
unsigned long signalTimer=0;

uint8_t brightness = 100; 
uint8_t maxFadeBrightness = 70; 
unsigned long fadeTimeMillis = 3000; 
unsigned long offTimeMillis = 1000;  

// ================= FADING & DELAY VARIABLES =================
unsigned long lastFadeTime = 0;
unsigned long fadeInterval = 43; 
int fadeDirection = 1;                 
int currentFadeBrightness = 0;         
bool isOffPeriod = false;            
unsigned long offPeriodStartTime = 0; 

// ================= PATTERNS (LOGICAL ONLY) =================
int pattern[]   ={0,0,0,1,0,1,0,1,0,0,0};
int pattern24[]  ={1,0,1,0,1,0,0,0,0,0};
int pattern25[] ={0,0,0,0,0,1,0,1,0,1};

int patternIndex=0;
int pattern9Index=0;
int pattern10Index=0;

unsigned long lastPatternTime=0;
const unsigned long patternDelay=80;

// ================= RELAY WRITE =================
void setRelayPin(int pin, bool state){
#if RELAY_ACTIVE_LOW 
    digitalWrite(pin, state ? LOW : HIGH); 
#else
    digitalWrite(pin, state ? HIGH : LOW); 
#endif
}

// ================= recalculateFadeInterval =================
void recalculateFadeInterval() {
    if(maxFadeBrightness > 0 && fadeTimeMillis > 0) {
        fadeInterval = fadeTimeMillis / maxFadeBrightness;
        if(fadeInterval == 0) fadeInterval = 1; 
    } else {
        fadeInterval = 43;
    }
}

// ================= APPLY =================
void applyRelays(){

    // Relay1 Logic with Relay 6 Front Led Combo
    if(relay6State && !signalActive) {
        setRelayPin(RELAY1, r6Btn1State);
    } 
    else if(relay5State && relay1State && !signalActive){
        setRelayPin(RELAY1, pattern24[pattern9Index]); 
    } else {
        setRelayPin(RELAY1, relay1State);
    }

    // Relay2
    if(relay4State && relay2State){
        setRelayPin(RELAY2, pattern[patternIndex]); 
    }else{
        setRelayPin(RELAY2, relay2State);
    }

    // Relay3 Logic with Relay 6 Rear Led Combo
    if(relay6State && !signalActive) {
        setRelayPin(RELAY3, r6Btn2State);
    }
    else if(relay5State && relay3State && !signalActive){
        setRelayPin(RELAY3, pattern25[pattern10Index]); 
    }else{
        setRelayPin(RELAY3, relay3State);
    }

    // Relay4
    setRelayPin(RELAY4, relay4State);

    // Relay5 & Relay6 Logic (SHARED PIN 10)
    if(signalActive){
        setRelayPin(10, false); 
    } 
    else if(relay6State) {
        setRelayPin(RELAY6, true); 
    } 
    else {
        setRelayPin(RELAY5, relay5State);
    }

    // Auto Helmet Off Logic (GPIO 1)
    setRelayPin(AUTO_OFF_PIN, autoOffState);
}

// ================= EEPROM =================
void saveState(){
    EEPROM.write(0, relay1State);
    EEPROM.write(1, relay2State);
    EEPROM.write(2, relay3State);
    EEPROM.write(3, relay4State);
    EEPROM.write(4, relay5State);
    EEPROM.write(5, brightness);
    EEPROM.write(6, relay6State);
    EEPROM.write(7, maxFadeBrightness); 
    EEPROM.write(8, r6Btn1State);
    EEPROM.write(9, r6Btn2State);
    
    EEPROM.put(10, fadeTimeMillis);    
    EEPROM.put(14, offTimeMillis);     
    
    EEPROM.write(18, autoOffState); // অ্যাড্রেস ১৮-এ অটো অফ মোড সেভ হচ্ছে
    EEPROM.commit();
}

void loadState(){
    relay1State=EEPROM.read(0);
    relay2State=EEPROM.read(1);
    relay3State=EEPROM.read(2);
    relay4State=EEPROM.read(3);
    relay5State=EEPROM.read(4);
    brightness=EEPROM.read(5);
    relay6State=EEPROM.read(6);
    maxFadeBrightness=EEPROM.read(7);
    r6Btn1State=EEPROM.read(8);
    r6Btn2State=EEPROM.read(9);
    
    EEPROM.get(10, fadeTimeMillis);    
    EEPROM.get(14, offTimeMillis);     
    
    autoOffState=EEPROM.read(18);

    if(brightness>100) brightness=100;
    if(maxFadeBrightness>100 || maxFadeBrightness == 0) maxFadeBrightness=70;
    if(fadeTimeMillis == 0 || fadeTimeMillis > 60000) fadeTimeMillis=3000; 
    if(offTimeMillis > 60000) offTimeMillis=1000; 
    if(autoOffState > 1) autoOffState=false;
    
    recalculateFadeInterval();
}

// ================= TOGGLE FUNCTIONS =================
void toggleRelay1(){relay1State=!relay1State; applyRelays(); saveState(); server.send(200,"text/plain","OK");}
void toggleRelay2(){relay2State=!relay2State; applyRelays(); saveState(); server.send(200,"text/plain","OK");}
void toggleRelay3(){relay3State=!relay3State; applyRelays(); saveState(); server.send(200,"text/plain","OK");}
void toggleRelay4(){relay4State=!relay4State; applyRelays(); saveState(); server.send(200,"text/plain","OK");}

void toggleRelay5(){
    relay5State=!relay5State;
    if(relay5State) {
        relay6State = false; 
        ledcWrite(LEDPIN, map(brightness, 0, 100, 0, 255)); 
    }
    applyRelays();
    saveState();
    server.send(200,"text/plain","OK");
}

void toggleRelay6(){
    relay6State=!relay6State;
    if(relay6State) {
        relay5State = false; 
        currentFadeBrightness = 0;
        fadeDirection = 1;
        isOffPeriod = false; 
    } else {
        ledcWrite(LEDPIN, map(brightness, 0, 100, 0, 255));
    }
    applyRelays(); 
    saveState();
    server.send(200,"text/plain","OK");
}

void toggleR6Btn1(){ r6Btn1State=!r6Btn1State; applyRelays(); saveState(); server.send(200,"text/plain","OK"); }
void toggleR6Btn2(){ r6Btn2State=!r6Btn2State; applyRelays(); saveState(); server.send(200,"text/plain","OK"); }

void toggleAutoOff(){
    autoOffState = !autoOffState;
    applyRelays();
    saveState();
    server.send(200, "text/plain", "OK");
}

void setBrightness(){
    if(server.hasArg("v")){
        brightness=constrain(server.arg("v").toInt(),0,100);
        if(!relay6State){
            ledcWrite(LEDPIN, map(brightness,0,100,0,255));
        }
        saveState();
    }
    server.send(200,"text/plain","OK");
}

void setRelay6Config(){
    if(server.hasArg("mb"))  maxFadeBrightness = constrain(server.arg("mb").toInt(), 1, 100);
    if(server.hasArg("ft")) {
        fadeTimeMillis = server.arg("ft").toInt(); 
        if(fadeTimeMillis < 50) fadeTimeMillis = 50; 
    }
    if(server.hasArg("oft")) offTimeMillis = server.arg("oft").toInt(); 
    
    recalculateFadeInterval();
    saveState();
    server.send(200, "text/plain", "OK");
}

// ================= SIGNAL =================
void checkSignal(){
    bool sig = digitalRead(SIGNAL_PIN);

    if(sig == HIGH){
        signalActive = true;
        waitingRestore = false;
        ledcWrite(LEDPIN, map(brightness, 0, 100, 0, 255)); 
    }
    else {
        if(lastSignal == HIGH){
            signalTimer = millis();
            waitingRestore = true;
        }

        if(waitingRestore){
            if(millis() - signalTimer >= 1000){
                signalActive = false;
                waitingRestore = false;
                
                if(!relay6State) {
                    ledcWrite(LEDPIN, map(brightness, 0, 100, 0, 255));
                }
            }
        }
    }

    lastSignal = sig;
    applyRelays();
}

// ================= PWM FADE & OFF-TIME LOGIC =================
void handleRelay6Fade(){
    if(relay6State && !signalActive){
        
        if(isOffPeriod) {
            if(millis() - offPeriodStartTime >= offTimeMillis) {
                isOffPeriod = false; 
                fadeDirection = 1;   
                lastFadeTime = millis();
            }
            return; 
        }

        if(millis() - lastFadeTime >= fadeInterval){
            lastFadeTime = millis();
            currentFadeBrightness += fadeDirection;
            
            if(currentFadeBrightness >= maxFadeBrightness){
                currentFadeBrightness = maxFadeBrightness;
                fadeDirection = -1; 
            }
            else if(currentFadeBrightness <= 0){
                currentFadeBrightness = 0;
                
                if(offTimeMillis > 0) {
                    isOffPeriod = true;
                    offPeriodStartTime = millis();
                    ledcWrite(LEDPIN, 0); 
                    return;
                } else {
                    fadeDirection = 1; 
                }
            }
            
            if(!isOffPeriod) {
                ledcWrite(LEDPIN, map(currentFadeBrightness, 0, 100, 0, 255));
            }
        }
    }
}

// ================= RUN PATTERN (LOGICAL ONLY) =================
void runPattern(){
    if(millis() - lastPatternTime >= patternDelay){
        lastPatternTime = millis();

        patternIndex++;
        if(patternIndex >= 11) patternIndex = 0;

        pattern9Index++;
        if(pattern9Index >= 10) pattern9Index = 0;

        pattern10Index++;
        if(pattern10Index >= 10) pattern10Index = 0;
    }
}

// ================= STATUS =================
void sendStatus(){
    String json="{";
    json+="\"r1\":"+String(relay1State);
    json+=",\"r2\":"+String(relay2State);
    json+=",\"r3\":"+String(relay3State);
    json+=",\"r4\":"+String(relay4State);
    json+=",\"r5\":"+String(relay5State);
    json+=",\"r6\":"+String(relay6State);
    json+=",\"b1\":"+String(r6Btn1State); 
    json+=",\"b2\":"+String(r6Btn2State); 
    json+=",\"sig\":"+String(signalActive);
    json+=",\"br\":"+String(brightness);
    json+=",\"mb\":"+String(maxFadeBrightness);
    json+=",\"ft\":"+String(fadeTimeMillis); 
    json+=",\"oft\":"+String(offTimeMillis); 
    json+=",\"ao\":"+String(autoOffState); // স্ট্যাটাসে অটো অফ যুক্ত হলো
    json        +="}";
    server.send(200,"application/json",json);
}

// ================= WEB PAGE =================
// =====================================================
// UPDATED WEB PAGE CODE ONLY (WITH DYNAMIC TEXT TOGGLE)
// =====================================================
void handleRoot(){
    String html=R"====(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
    background: #090d16;
    color: #f1f5f9;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    text-align: center;
    padding: 20px 15px;
    position: relative;
    overflow-x: hidden;
}
.header-bar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    max-width: 440px;
    margin: 0 auto 25px auto;
}
h2 {
    font-size: 22px;
    font-weight: 600;
    color: #38bdf8;
    text-transform: uppercase;
    letter-spacing: 1px;
    text-shadow: 0 0 15px rgba(56, 189, 248, 0.3);
}
.burger-menu {
    font-size: 28px;
    color: #38bdf8;
    cursor: pointer;
    user-select: none;
    padding: 5px;
    transition: transform 0.2s;
}
.burger-menu:active { transform: scale(0.9); }

.side-panel {
    position: fixed;
    top: 0;
    right: -260px; 
    width: 250px;
    height: 100%;
    background: rgba(15, 23, 42, 0.95);
    backdrop-filter: blur(15px);
    box-shadow: -5px 0 25px rgba(0,0,0,0.5);
    z-index: 100;
    transition: right 0.3s ease-in-out;
    padding: 30px 20px;
    text-align: left;
    border-left: 1px solid rgba(255, 255, 255, 0.05);
}
.side-panel.open { right: 0; }
.side-panel h4 { font-size: 16px; color: #94a3b8; margin-bottom: 25px; border-bottom: 1px solid rgba(255,255,255,0.1); padding-bottom: 10px; text-transform: uppercase; }
.close-btn { font-size: 24px; color: #ef4444; float: right; cursor: pointer; margin-top: -5px; }

.menu-btn {
    width: 100%;
    padding: 14px;
    border-radius: 12px;
    border: 1px solid #334155;
    background: #1e293b;
    color: #ef4444;
    font-size: 14px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.3s ease;
    display: flex;
    justify-content: center;
    align-items: center;
    box-shadow: 0 4px 6px rgba(0,0,0,0.1);
}
.menu-btn.active {
    background: #10b981;
    color: #090d16;
    border-color: #10b981;
    box-shadow: 0 0 15px rgba(16, 185, 129, 0.5);
}

.container { max-width: 440px; margin: 0 auto; }
.wrap { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 25px; }
.card {
    background: rgba(30, 41, 59, 0.7);
    backdrop-filter: blur(10px);
    border: 1px solid rgba(255, 255, 255, 0.05);
    padding: 16px;
    border-radius: 20px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
    transition: all 0.3s ease;
}
.card h3 { font-size: 12px; font-weight: 600; color: #94a3b8; margin-bottom: 12px; letter-spacing: 0.5px; text-transform: uppercase; }
.logo { width: 85px; height: 85px; cursor: pointer; border-radius: 50%; overflow: hidden; border: 3px solid #334155; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); }
.logo img { width: 100%; height: 100%; object-fit: cover; }
.on { border-color: #10b981; box-shadow: 0 0 20px rgba(16, 185, 129, 0.6); transform: scale(1.03); }
.off { border-color: #ef4444; box-shadow: 0 0 10px rgba(239, 68, 68, 0.2); }
.middle { grid-column: 1 / span 2; flex-direction: row; justify-content: space-between; padding: 14px 24px; }
.middle h3 { margin-bottom: 0; text-align: left; }
.middle .logo { width: 65px; height: 65px; }
.extra-btn-container { display: flex; gap: 10px; width: 100%; margin-top: 15px; border-top: 1px solid rgba(255, 255, 255, 0.08); padding-top: 12px; }
.sub-btn { flex: 1; padding: 10px; border-radius: 10px; border: 1px solid #334155; background: #1e293b; color: #94a3b8; font-size: 12px; font-weight: 600; cursor: pointer; transition: all 0.2s; }
.sub-btn.active { background: #10b981; color: #090d16; border-color: #10b981; box-shadow: 0 0 10px rgba(16, 185, 129, 0.4); }
.config-area { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; align-items: flex-end; margin-top: 12px; width: 100%; border-top: 1px solid rgba(255, 255, 255, 0.08); padding-top: 12px; }
.input-box { display: flex; flex-direction: column; align-items: flex-start; gap: 4px; }
.input-box label { font-size: 9px; font-weight: 600; color: #64748b; text-transform: uppercase; letter-spacing: 0.3px; white-space: nowrap; }
.config-area input { width: 100%; background: #0f172a; border: 1px solid #334155; color: white; padding: 7px 4px; border-radius: 8px; font-size: 11px; text-align: center; outline: none; }
.set-btn-wrapper { grid-column: 1 / span 3; margin-top: 4px; }
.set-btn-wrapper button { width: 100%; background: #38bdf8; border: none; color: #090d16; padding: 8px 0; border-radius: 8px; font-size: 12px; font-weight: bold; cursor: pointer; transition: background 0.2s; }
.set-btn-wrapper button:active { background: #0ea5e9; }
.control-panel { background: rgba(15, 23, 42, 0.8); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 24px; padding: 20px; margin-top: 10px; }
.panel-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
.panel-header h3 { font-size: 14px; color: #94a3b8; }
.led { width: 14px; height: 14px; border-radius: 50%; display: inline-block; background: #ef4444; box-shadow: 0 0 8px rgba(239, 68, 68, 0.5); transition: all 0.3s; }
.ledon { background: #10b981; box-shadow: 0 0 15px #10b981; }
.slider-container { position: relative; width: 100%; height: 48px; background: #1e293b; border-radius: 14px; overflow: hidden; display: flex; align-items: center; }
input[type='range'] { -webkit-appearance: none; width: 100%; height: 100%; background: transparent; outline: none; position: absolute; top: 0; left: 0; z-index: 2; cursor: pointer; }
.slider-bar { position: absolute; top: 0; left: 0; height: 100%; background: #38bdf8; width: 100%; z-index: 1; pointer-events: none; transition: width 0.05s linear; }
.slider-text { position: absolute; width: 100%; display: flex; justify-content: space-between; padding: 0 15px; z-index: 3; pointer-events: none; font-size: 14px; font-weight: 600; color: #ffffff; mix-blend-mode: difference; }
input[type='range']::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 50px; }
input[type='range']::-moz-range-thumb { width: 20px; height: 50px; background: transparent; border: none; }
</style>
</head>
<body>

<div class="header-bar">
    <h2>Helmet LED Control</h2>
    <div class="burger-menu" onclick="toggleMenu()">*</div>
</div>

<div id="sidePanel" class="side-panel">
    <span class="close-btn" onclick="toggleMenu()">*</span>
    <h4>Home</h4>
    <button id="aoBtn" class="menu-btn" onclick="tgAutoOff()">Auto Helmet Off</button>
</div>

<div class="container">
    <div class='wrap'>
        <div class='card'><h3>Only Front</h3><div id='b1' class='logo off' onclick='tg(1)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Front%20Led.png'></div></div>
        <div class='card'><h3>Only Back</h3><div id='b2' class='logo off' onclick='tg(2)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Back%20Led.jpg'></div></div>
        <div class='card'><h3>Only Rear</h3><div id='b3' class='logo off' onclick='tg(3)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Rear%20Led.jpeg'></div></div>
        <div class='card'><h3>Back Burst</h3><div id='b4' class='logo off' onclick='tg(4)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Back%20Burst.jpg'></div></div>
        <div class='card middle'><h3>Indicator Burst</h3><div id='b5' class='logo off' onclick='tg(5)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/INDICATOR%20BURST.jpg'></div></div>
        <div class='card middle' style='flex-direction: column; height: auto;'>
            <div style='display: flex; justify-content: space-between; width: 100%; align-items: center;'>
                <h3>Helmet Led Eye</h3>
                <div id='b6' class='logo' onclick='tg(6)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/HELMET%20LED%20EYE.png'></div>
            </div>
            <div class="extra-btn-container">
                <button id="ex1" class="sub-btn" onclick="tgExtra(1)">Front Led</button>
                <button id="ex2" class="sub-btn" onclick="tgExtra(2)">Rear Led</button>
            </div>
            <div class='config-area'>
                <div class="input-box"><label for="maxB">Brightness</label><input type='number' id='maxB' min='1' max='100' placeholder='Max %'></div>
                <div class="input-box"><label for="fTime">On Time (ms)</label><input type='number' id='fTime' min='50' max='60000' placeholder='On ms'></div>
                <div class="input-box"><label for="ofTime">Off Time (ms)</label><input type='number' id='ofTime' min='0' max='60000' placeholder='Off ms'></div>
                <div class="set-btn-wrapper"><button onclick='saveCfg()'>SET CONFIGURATION</button></div>
            </div>
        </div>
    </div>
    <div class='control-panel'>
        <div class="panel-header"><h3>SYSTEM STATUS</h3><div>Signal: <span id='sig' class='led'></span></div></div>
        <div class='slider-container'>
            <input type='range' min='0' max='100' value='100' id='br' oninput='setBr(this.value)'>
            <div id='bar' class='slider-bar'></div>
            <div class='slider-text'><span>BRIGHTNESS</span><span id='brtxt'>100%</span></div>
        </div>
    </div>
</div>
<script>
function toggleMenu() {
    var panel = document.getElementById("sidePanel");
    panel.classList.toggle("open");
}
function tg(x){
    fetch('/toggle'+x).then(() => {
        if(x === 5) { document.getElementById('b6').className = 'logo off'; }
        if(x === 6) { document.getElementById('b5').className = 'logo off'; }
        setTimeout(up, 80); 
    });
}
function tgExtra(x) { fetch('/toggleR6Btn' + x).then(() => { setTimeout(up, 80); }); }
function tgAutoOff() { fetch('/toggleAutoOff').then(() => { setTimeout(up, 80); }); }

function saveCfg(){
    var mb = document.getElementById('maxB').value;
    var ft = document.getElementById('fTime').value;
    var oft = document.getElementById('ofTime').value;
    if(mb && ft && oft){
        fetch('/setr6?mb=' + mb + '&ft=' + ft + '&oft=' + oft).then(() => { alert("Configuration Saved!"); up(); });
    } else { alert("Please fill Brightness, On Time, and Off Time!"); }
}
function st(id,v){ document.getElementById(id).className = v ? 'logo on' : 'logo off'; }
function setBr(v){
    document.getElementById('brtxt').innerHTML = v + '%';
    document.getElementById('bar').style.width = v + '%';
    fetch('/brightness?v=' + v);
}
function up(){
    fetch('/status').then(r=>r.json()).then(d=>{
        st('b1',d.r1); st('b2',d.r2); st('b3',d.r3); st('b4',d.r4); st('b5',d.r5); st('b6',d.r6);
        document.getElementById('ex1').className = d.b1 ? 'sub-btn active' : 'sub-btn';
        document.getElementById('ex2').className = d.b2 ? 'sub-btn active' : 'sub-btn';
        document.getElementById('sig').className = d.sig ? 'led ledon' : 'led';
        document.getElementById('br').value = d.br;
        document.getElementById('brtxt').innerHTML = d.br + '%';
        document.getElementById('bar').style.width = d.br + '%';
        
        // ডাইনামিক টেক্সট এবং ক্লাস চেঞ্জ লজিক
        var aoBtn = document.getElementById('aoBtn');
        aoBtn.className = d.ao ? 'menu-btn active' : 'menu-btn';
        aoBtn.textContent = d.ao ? "Auto Helmet On" : "Auto Helmet Off";

        if(document.activeElement.id !== 'maxB') document.getElementById('maxB').value = d.mb;
        if(document.activeElement.id !== 'fTime') document.getElementById('fTime').value = d.ft;
        if(document.activeElement.id !== 'ofTime') document.getElementById('ofTime').value = d.oft;
    }).catch(err => console.log("Sync error"));
}
setInterval(up,1000);
up();
</script>
</body>
</html>
)====";
    server.send(200,"text/html",html);
}
    
// ================= SETUP =================
void setup(){
    Serial.begin(115200);
    EEPROM.begin(64);

    pinMode(RELAY1,OUTPUT);
    pinMode(RELAY2,OUTPUT);
    pinMode(RELAY3,OUTPUT);
    pinMode(RELAY4,OUTPUT);
    pinMode(RELAY5,OUTPUT); 
    pinMode(RELAY6,OUTPUT); 
    pinMode(AUTO_OFF_PIN, OUTPUT); // জিপিআইও ১ আউটপুট কনফিগার করা হলো

    pinMode(LEDPIN,OUTPUT);
    pinMode(SIGNAL_PIN,INPUT);

    ledcAttach(LEDPIN, PWM_FREQ, PWM_RESOLUTION);

    loadState();
    
    if(!relay6State){
        ledcWrite(LEDPIN,map(brightness,0,100,0,255));
    }
    
    applyRelays();

    WiFi.begin(ssid,password);

    while(WiFi.status()!=WL_CONNECTED){
        runPattern();
        checkSignal();
        handleRelay6Fade(); 
        delay(1);
    }

    server.on("/",handleRoot);
    server.on("/toggle1",toggleRelay1);
    server.on("/toggle2",toggleRelay2);
    server.on("/toggle3",toggleRelay3);
    server.on("/toggle4",toggleRelay4);
    server.on("/toggle5",toggleRelay5);
    server.on("/toggle6",toggleRelay6);
    server.on("/toggleAutoOff", toggleAutoOff); // নতুন রাউট এড করা হলো
    
    server.on("/toggleR6Btn1",toggleR6Btn1);
    server.on("/toggleR6Btn2",toggleR6Btn2);
    
    server.on("/setr6",setRelay6Config); 
    server.on("/status",sendStatus);
    server.on("/brightness",setBrightness);

    server.begin();
}

// ================= LOOP =================
void loop(){
    runPattern();
    checkSignal();
    handleRelay6Fade(); 
    server.handleClient();
}
