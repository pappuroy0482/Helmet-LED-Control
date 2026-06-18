// ====================================================================
// ESP32-C3 FULL CODE - FIXED LOGIC WITH EYE PATTERNS & WEB INTERFACE
// ====================================================================

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
#define RELAY5 6
#define RELAY6 6    
#define AUTO_OFF_PIN 10      // অটো হেলমেট অফ মোডের ফিজিক্যাল আউটপুট পিন
#define RELAY_LOW_BEAM 1   // নতুন ১০ নম্বর পিন (Back Low Beam)

#define LEDPIN 8            // Brightness PWM Pin
#define SIGNAL_PIN 7        // Signal Input Pin

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
bool autoOffState=false; 
bool lowBeamState=false;   

// ড্রপডাউন মেমোরি স্টেট
uint8_t selBackPattern = 1;
uint8_t selFrontRearPattern = 1;
uint8_t selEyePattern = 1; 

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

// ================= FADING & EYE PATTERN VARIABLES =================
unsigned long lastFadeTime = 0;
unsigned long fadeInterval = 43; 
int fadeDirection = 1;                 
int currentFadeBrightness = 0;         
bool isOffPeriod = false;            
unsigned long offPeriodStartTime = 0; 
int eyePatternStep = 0; 

// ================= 5 PATTERNS FOR ONLY BACK (Size 11) =================
int p_back_1[] = {0,0,0,1,0,1,0,1,0,0,0};
int p_back_2[] = {1,1,0,0,1,1,0,0,1,1,0};
int p_back_3[] = {1,0,1,0,1,0,1,0,1,0,1};
int p_back_4[] = {1,1,1,1,0,0,0,0,1,1,1};
int p_back_5[] = {1,0,0,0,0,1,1,1,1,1,0};

// ================= 5 PATTERNS FOR FRONT & REAR (Size 10) =================
int p_f24_1[] = {1,0,1,0,1,0,0,0,0,0};
int p_r25_1[] = {0,0,0,0,0,1,0,1,0,1};

int p_f24_2[] = {1,1,1,0,0,0,1,1,1,0};
int p_r25_2[] = {0,0,0,1,1,1,0,0,0,1};

int p_f24_3[] = {1,0,0,1,0,0,1,0,0,1};
int p_r25_3[] = {0,1,1,0,1,1,0,1,1,0};

int p_f24_4[] = {1,1,0,0,1,1,0,0,1,1};
int p_r25_4[] = {1,1,0,0,1,1,0,0,1,1};

int p_f24_5[] = {1,0,1,1,0,1,0,0,1,1};
int p_r25_5[] = {0,1,0,0,1,0,1,1,0,0};

// রানটাইম পয়েন্টার হোল্ডার
int* pattern   = p_back_1;
int* pattern24 = p_f24_1;
int* pattern25 = p_r25_1;

int patternIndex=0;
int pattern9Index=0;
int pattern10Index=0;

unsigned long lastBackPatternTime = 0;
unsigned long backPatternDelay = 80;

unsigned long lastFRPatternTime = 0;
unsigned long frPatternDelay = 80;

// ================= RELAY WRITE =================
void setRelayPin(int pin, bool state){
#if RELAY_ACTIVE_LOW 
    digitalWrite(pin, state ? LOW : HIGH); 
#else
    digitalWrite(pin, state ? HIGH : LOW); 
#endif
}

void updatePatternPointers() {
    if (selBackPattern == 2)       pattern = p_back_2;
    else if (selBackPattern == 3)  pattern = p_back_3;
    else if (selBackPattern == 4)  pattern = p_back_4;
    else if (selBackPattern == 5)  pattern = p_back_5;
    else                           pattern = p_back_1;

    if (selFrontRearPattern == 2) {
        pattern24 = p_f24_2;
        pattern25 = p_r25_2;
    } else if (selFrontRearPattern == 3) {
        pattern24 = p_f24_3;
        pattern25 = p_r25_3;
    } else if (selFrontRearPattern == 4) {
        pattern24 = p_f24_4;
        pattern25 = p_r25_4;
    } else if (selFrontRearPattern == 5) {
        pattern24 = p_f24_5;
        pattern25 = p_r25_5;
    } else {
        pattern24 = p_f24_1;
        pattern25 = p_r25_1;
    }
}

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
    if(relay6State && !signalActive) {
        setRelayPin(RELAY1, r6Btn1State);
    } 
    else if(relay5State && relay1State && !signalActive){
        setRelayPin(RELAY1, pattern24[pattern9Index]); 
    } else {
        setRelayPin(RELAY1, relay1State);
    }

    if(relay4State && relay2State){
        setRelayPin(RELAY2, pattern[patternIndex]); 
    }else{
        setRelayPin(RELAY2, relay2State);
    }

    if(relay6State && !signalActive) {
        setRelayPin(RELAY3, r6Btn2State);
    }
    else if(relay5State && relay3State && !signalActive){
        setRelayPin(RELAY3, pattern25[pattern10Index]); 
    }else{
        setRelayPin(RELAY3, relay3State);
    }

    setRelayPin(RELAY4, relay4State);

    if(signalActive){
        setRelayPin(RELAY5, false); 
    } 
    else if(relay6State) {
        setRelayPin(RELAY6, true); 
    } 
    else {
        setRelayPin(RELAY5, relay5State);
    }

    setRelayPin(AUTO_OFF_PIN, autoOffState);
    setRelayPin(RELAY_LOW_BEAM, lowBeamState);
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
    
    EEPROM.write(18, autoOffState); 
    EEPROM.write(20, selBackPattern);       
    EEPROM.write(21, selFrontRearPattern); 
    
    EEPROM.put(22, backPatternDelay); 
    EEPROM.put(26, frPatternDelay);   
    EEPROM.write(29, lowBeamState);  
    EEPROM.write(30, selEyePattern); 
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
    selBackPattern=EEPROM.read(20);
    selFrontRearPattern=EEPROM.read(21);
    
    EEPROM.get(22, backPatternDelay);
    EEPROM.get(26, frPatternDelay);
    lowBeamState=EEPROM.read(29);    
    selEyePattern=EEPROM.read(30);   

    if(brightness>100) brightness=100;
    if(maxFadeBrightness>100 || maxFadeBrightness == 0) maxFadeBrightness=70;
    if(fadeTimeMillis == 0 || fadeTimeMillis > 60000) fadeTimeMillis=3000; 
    if(offTimeMillis > 60000) offTimeMillis=1000; 
    if(autoOffState > 1) autoOffState=false;
    if(lowBeamState > 1) lowBeamState=false;
    if(selEyePattern < 1 || selEyePattern > 5) selEyePattern = 1;
    
    if(selBackPattern < 1 || selBackPattern > 5) selBackPattern = 1;
    if(selFrontRearPattern < 1 || selFrontRearPattern > 5) selFrontRearPattern = 1;
    if(backPatternDelay == 0 || backPatternDelay > 5000) backPatternDelay = 80;
    if(frPatternDelay == 0 || frPatternDelay > 5000) frPatternDelay = 80;
    
    updatePatternPointers();
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
        eyePatternStep = 0;
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

void toggleLowBeam(){
    lowBeamState = !lowBeamState;
    applyRelays();
    saveState();
    server.send(200, "text/plain", "OK");
}

void setBackPatternRoute(){
    if(server.hasArg("v")){
        selBackPattern = constrain(server.arg("v").toInt(), 1, 5);
        updatePatternPointers();
        applyRelays();
        saveState();
    }
    server.send(200, "text/plain", "OK");
}

void setFrontRearPatternRoute(){
    if(server.hasArg("v")){
        selFrontRearPattern = constrain(server.arg("v").toInt(), 1, 5);
        updatePatternPointers();
        applyRelays();
        saveState();
    }
    server.send(200, "text/plain", "OK");
}

void setEyePatternRoute(){
    if(server.hasArg("v")){
        selEyePattern = constrain(server.arg("v").toInt(), 1, 5);
        eyePatternStep = 0;
        saveState();
    }
    server.send(200, "text/plain", "OK");
}

void setPatternSpeedConfig(){
    if(server.hasArg("bs")) backPatternDelay = constrain(server.arg("bs").toInt(), 10, 5000);
    if(server.hasArg("frs")) frPatternDelay = constrain(server.arg("frs").toInt(), 10, 5000);
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

// ================= PWM MULTI-PATTERN LOGIC FOR HELMET EYE =================
void handleRelay6Fade(){
    if(relay6State && !signalActive){
        
        if(isOffPeriod) {
            if(millis() - offPeriodStartTime >= offTimeMillis) {
                isOffPeriod = false; 
                fadeDirection = 1;   
                eyePatternStep = 0;
                lastFadeTime = millis();
            }
            return; 
        }

        unsigned long currentInterval = (selEyePattern == 2) ? fadeInterval : (fadeTimeMillis / 10);

        if(millis() - lastFadeTime >= currentInterval){
            lastFadeTime = millis();
            int pwmVal = 0;

            switch(selEyePattern) {
                case 1: 
                    if(eyePatternStep < 6) {
                        pwmVal = (eyePatternStep % 2 == 0) ? maxFadeBrightness : 0;
                        eyePatternStep++;
                    } else {
                        isOffPeriod = true;
                        offPeriodStartTime = millis();
                    }
                    break;

                case 2: 
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
                    pwmVal = currentFadeBrightness;
                    break;

                case 3: 
                    if(eyePatternStep < 4) {
                        pwmVal = (eyePatternStep % 2 == 0) ? maxFadeBrightness : 0;
                        eyePatternStep++;
                    } else {
                        isOffPeriod = true;
                        offPeriodStartTime = millis();
                    }
                    break;

                case 4: 
                    if(eyePatternStep == 0 || eyePatternStep == 1 || eyePatternStep == 2) pwmVal = maxFadeBrightness; 
                    else if(eyePatternStep == 3) pwmVal = 0;
                    else if(eyePatternStep == 4) pwmVal = maxFadeBrightness; 
                    else if(eyePatternStep == 5) pwmVal = 0;
                    else if(eyePatternStep == 6) pwmVal = maxFadeBrightness; 
                    else {
                        isOffPeriod = true;
                        offPeriodStartTime = millis();
                    }
                    eyePatternStep++;
                    break;

                case 5: 
                    pwmVal = random(maxFadeBrightness / 3, maxFadeBrightness);
                    eyePatternStep++;
                    if(eyePatternStep > 15) {
                        isOffPeriod = true;
                        offPeriodStartTime = millis();
                    }
                    break;
            }
            
            if(!isOffPeriod) {
                ledcWrite(LEDPIN, map(pwmVal, 0, 100, 0, 255));
            }
        }
    }
}

// ================= RUN PATTERN INDEPENDENT LOGIC =================
void runPattern(){
    unsigned long currentMillis = millis();
    
    if(currentMillis - lastBackPatternTime >= backPatternDelay){
        lastBackPatternTime = currentMillis;
        patternIndex++;
        if(patternIndex >= 11) patternIndex = 0;
    }

    if(currentMillis - lastFRPatternTime >= frPatternDelay){
        lastFRPatternTime = currentMillis;
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
    json+=",\"ao\":"+String(autoOffState); 
    json+=",\"lb\":"+String(lowBeamState);   
    json+=",\"bp\":"+String(selBackPattern);       
    json+=",\"fr\":"+String(selFrontRearPattern); 
    json+=",\"ep\":"+String(selEyePattern); 
    json+=",\"bs\":"+String(backPatternDelay);  
    json+=",\"frs\":"+String(frPatternDelay);   
    json        +="}";
    server.send(200,"application/json",json);
}

// ================= WEB PAGE =================
void handleRoot(){
    String html=R"====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>
<title>Helmet LED Control</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }

body {
    background: radial-gradient(circle at top, #141b2d 0%, #090d16 100%);
    color: #f8fafc;
    text-align: center;
    padding: 25px 16px;
    overflow-x: hidden;
    min-height: 100vh;
}

.header-bar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    max-width: 440px;
    margin: 0 auto 25px auto;
    padding: 0 5px;
}

h2 {
    font-size: 20px;
    font-weight: 700;
    color: #38bdf8;
    text-transform: uppercase;
    letter-spacing: 1.5px;
    text-shadow: 0 0 20px rgba(56, 189, 248, 0.4);
}

.burger-menu {
    font-size: 28px;
    color: #38bdf8;
    cursor: pointer;
    user-select: none;
    transition: transform 0.2s;
    line-height: 1;
}
.burger-menu:active { transform: scale(0.85); }

.side-panel {
    position: fixed;
    top: 0;
    right: -290px; 
    width: 280px;
    height: 100%;
    background: rgba(11, 15, 25, 0.97);
    backdrop-filter: blur(25px);
    -webkit-backdrop-filter: blur(25px);
    box-shadow: -10px 0 35px rgba(0,0,0,0.7);
    z-index: 100;
    transition: right 0.4s cubic-bezier(0.16, 1, 0.3, 1);
    padding: 25px 20px;
    text-align: left;
    border-left: 1px solid rgba(255, 255, 255, 0.08);
    display: flex;
    flex-direction: column;
    overflow-y: auto;
}
.side-panel.open { right: 0; }
.side-panel h4 { font-size: 14px; color: #94a3b8; margin-bottom: 20px; border-bottom: 1px solid rgba(255,255,255,0.1); padding-bottom: 12px; text-transform: uppercase; letter-spacing: 1px; }
.close-btn { font-size: 28px; color: #94a3b8; float: right; cursor: pointer; margin-top: -8px; line-height: 1; }

.dropdown-box {
    margin-bottom: 22px;
    display: flex;
    flex-direction: column;
    gap: 8px;
    background: rgba(255, 255, 255, 0.03);
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(255, 255, 255, 0.05);
}
.dropdown-box label {
    font-size: 10px;
    font-weight: 700;
    color: #38bdf8;
    text-transform: uppercase;
    letter-spacing: 1px;
}
.pattern-select {
    width: 100%;
    padding: 12px;
    background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
    border: 1px solid rgba(56, 189, 248, 0.25);
    color: #fff;
    font-size: 12px;
    font-weight: 600;
    border-radius: 12px;
    outline: none;
    cursor: pointer;
    box-shadow: inset 0 1px 2px rgba(255,255,255,0.1), 0 4px 10px rgba(0,0,0,0.4);
}
.speed-input-container {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    margin-top: 6px;
}
.speed-input-container span { font-size: 10px; color: #94a3b8; font-weight: 600; text-transform: uppercase;}
.speed-input-container input {
    width: 90px;
    background: #0b0f19;
    border: 1px solid rgba(255,255,255,0.15);
    color: #fff;
    padding: 8px;
    border-radius: 8px;
    font-size: 12px;
    text-align: center;
    outline: none;
}
.speed-input-container input:focus { border-color: #38bdf8; }

.speed-submit-btn {
    width: 100%;
    background: linear-gradient(135deg, #38bdf8 0%, #0284c7 100%);
    border: none;
    color: #090d16;
    padding: 10px 0;
    border-radius: 10px;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.5px;
    cursor: pointer;
    margin-top: 4px;
    box-shadow: 0 4px 12px rgba(56, 189, 248, 0.2);
}

.menu-btn {
    width: 100%;
    padding: 14px 10px;
    border-radius: 14px;
    font-size: 11px; 
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    cursor: pointer;
    transition: all 0.15s ease-in-out;
    outline: none;
    border: 1px solid #7f1d1d;
    background: linear-gradient(135deg, #ef4444 0%, #991b1b 100%);
    color: #ffffff;
    box-shadow: 0 5px 0 #7f1d1d, 0 6px 12px rgba(0,0,0,0.4), inset 0 2px 3px rgba(255,255,255,0.3);
    text-shadow: 0 1px 2px rgba(0,0,0,0.6);
    line-height: 1.3;
    margin-bottom: 20px;
}
.menu-btn:active {  transform: translateY(3px); box-shadow: 0 2px 0 #7f1d1d, 0 3px 6px rgba(0,0,0,0.4), inset 0 2px 4px rgba(0,0,0,0.4); }
.menu-btn.active {
    border: 1px solid #064e3b;
    background: linear-gradient(135deg, #10b981 0%, #065f46 100%);
    color: #ffffff;
    box-shadow: 0 5px 0 #064e3b, 0 6px 15px rgba(16, 185, 129, 0.3), inset 0 2px 3px rgba(255,255,255,0.4);
}
.menu-btn.active:active { transform: translateY(3px); box-shadow: 0 2px 0 #064e3b, 0 3px 6px rgba(0,0,0,0.4), inset 0 2px 4px rgba(0,0,0,0.5); }

.note-container {
    margin-top: 15px; 
    background: rgba(15, 23, 42, 0.8);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 14px;
    padding: 10px;
    height: 130px;
    display: flex;
    flex-direction: column;
    box-shadow: inset 0 2px 8px rgba(0,0,0,0.5);
}
.note-title { font-size: 11px; font-weight: 700; color: #38bdf8; text-transform: uppercase; margin-bottom: 6px; }
.note-iframe { width: 100%; height: 100%; border: none; background: #090d16; color: #38bdf8; border-radius: 8px; padding: 5px; }

.container { max-width: 440px; margin: 0 auto; }
.wrap { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 20px; }

.card {
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    border: 1px solid rgba(255, 255, 255, 0.06);
    padding: 20px 14px;
    border-radius: 24px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
    transition: transform 0.2s;
}
.card h3 { font-size: 11px; font-weight: 600; color: #cbd5e1; margin-bottom: 16px; letter-spacing: 0.8px; text-transform: uppercase; text-shadow: 0 1px 4px rgba(0,0,0,0.5); }

.panel-blue { background: linear-gradient(135deg, rgba(56, 189, 248, 0.1), rgba(15, 23, 42, 0.6)); }
.panel-red { background: linear-gradient(135deg, rgba(239, 68, 68, 0.1), rgba(15, 23, 42, 0.6)); }
.panel-purple { background: linear-gradient(135deg, rgba(168, 85, 247, 0.1), rgba(15, 23, 42, 0.6)); }
.panel-orange { background: linear-gradient(135deg, rgba(249, 115, 22, 0.1), rgba(15, 23, 42, 0.6)); }

.logo { 
    width: 88px; 
    height: 88px; 
    cursor: pointer; 
    border-radius: 50%; 
    overflow: hidden; 
    transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); 
    background: #0f172a;
    padding: 3px;
    border: 3px solid #334155;
}
.logo img { width: 100%; height: 100%; object-fit: cover; border-radius: 50%; }

#b1.off { border-color: rgba(56, 189, 248, 0.3); }
#b1.on { border-color: #38bdf8; box-shadow: 0 0 25px rgba(56, 189, 248, 0.8); transform: scale(1.04); }
#b2.off { border-color: rgba(239, 68, 68, 0.3); }
#b2.on { border-color: #ef4444; box-shadow: 0 0 25px rgba(239, 68, 68, 0.8); transform: scale(1.04); }
#b3.off { border-color: rgba(168, 85, 247, 0.3); }
#b3.on { border-color: #a855f7; box-shadow: 0 0 25px rgba(168, 85, 247, 0.8); transform: scale(1.04); }
#b7.off { border-color: rgba(249, 115, 22, 0.3); }
#b7.on { border-color: #f97316; box-shadow: 0 0 25px rgba(249, 115, 22, 0.8); transform: scale(1.04); }

.card.middle-yellow { 
    grid-column: 1 / span 2; 
    flex-direction: column; 
    align-items: center;
    padding: 18px 24px; 
    background: linear-gradient(135deg, rgba(234, 179, 8, 0.08), rgba(15, 23, 42, 0.6)); 
}
.combo-header { width: 100%; text-align: center; margin-bottom: 12px; }
.combo-body { display: flex; justify-content: space-around; width: 100%; align-items: center; gap: 15px; }
.combo-item { display: flex; flex-direction: column; align-items: center; gap: 5px; }
.combo-item span { font-size: 10px; color: #cbd5e1; text-transform: uppercase; }

#b5 { width: 72px; height: 72px; }
#b5.off { border-color: rgba(234, 179, 8, 0.3); }
#b5.on { border-color: #eab308; box-shadow: 0 0 25px rgba(234, 179, 8, 0.8); transform: scale(1.04); }

#b4 { width: 72px; height: 72px; }
#b4.off { border-color: rgba(249, 115, 22, 0.3); }
#b4.on { border-color: #f97316; box-shadow: 0 0 25px rgba(249, 115, 22, 0.8); transform: scale(1.04); }

.card.middle-green { grid-column: 1 / span 2; flex-direction: column; height: auto; background: linear-gradient(135deg, rgba(16, 185, 129, 0.08), rgba(15, 23, 42, 0.7)); }
#b6 { width: 72px; height: 72px; }
#b6.off { border-color: rgba(16, 185, 129, 0.3); }
#b6.on { border-color: #10b981; box-shadow: 0 0 25px rgba(16, 185, 129, 0.8); transform: scale(1.04); }

.extra-btn-container { display: flex; gap: 14px; width: 100%; margin-top: 18px; border-top: 1px solid rgba(255, 255, 255, 0.08); padding-top: 16px; }
.sub-btn { 
    flex: 1; padding: 14px; border-radius: 14px; border: 1px solid rgba(255, 255, 255, 0.1); 
    background: linear-gradient(135deg, #242f47 0%, #111827 100%); color: #94a3b8; font-size: 13px; font-weight: 700; 
    letter-spacing: 0.5px; cursor: pointer; transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
    box-shadow: inset 0 2px 4px rgba(255,255,255,0.05), 0 4px 10px rgba(0,0,0,0.4); text-transform: uppercase;
}
.sub-btn:active { transform: translateY(2px); box-shadow: inset 0 2px 4px rgba(0,0,0,0.4); }
.sub-btn.active { 
    background: linear-gradient(135deg, #10b981 0%, #047857 100%); color: #04110b; border-color: #10b981; 
    box-shadow: 0 0 15px rgba(16, 185, 129, 0.5), inset 0 2px 2px rgba(255,255,255,0.2); 
}

.eye-pattern-container { width: 100%; margin-top: 14px; display: flex; flex-direction: column; gap: 6px; text-align: left;}
.eye-pattern-container label { font-size: 10px; font-weight: 700; color: #10b981; text-transform: uppercase; letter-spacing: 0.5px; }

.config-area { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; align-items: flex-end; margin-top: 16px; width: 100%; border-top: 1px solid rgba(255, 255, 255, 0.08); padding-top: 16px; }
.input-box { display: flex; flex-direction: column; align-items: flex-start; gap: 6px; }
.input-box label { font-size: 10px; font-weight: 700; color: #94a3b8; text-transform: uppercase; letter-spacing: 0.5px; white-space: nowrap; }
.config-area input { 
    width: 100%; background: #0b0f19; border: 1px solid rgba(255,255,255,0.12); color: #fff; 
    padding: 12px 4px; border-radius: 12px; font-size: 12px; text-align: center; outline: none; box-shadow: inset 0 2px 4px rgba(0,0,0,0.5); }
.config-area input:focus { border-color: #10b981; } .set-btn-wrapper { grid-column: 1 / span 3; margin-top: 8px; }
.set-btn-wrapper button { width: 100%; background: linear-gradient(135deg, #10b981 0%, #059669 100%); border: none; color: #090d16; padding: 14px 0; border-radius: 12px; font-size: 13px; font-weight: 700; letter-spacing: 0.8px; cursor: pointer; box-shadow: 0 4px 15px rgba(16, 185, 129, 0.3); } .control-panel { background: rgba(15, 23, 42, 0.6); backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.06); border-radius: 28px; padding: 20px; margin-top: 15px; } .panel-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding: 0 4px; } .panel-header h3 { font-size: 13px; color: #94a3b8; letter-spacing: 0.5px; } .status-container { display: flex; align-items: center; gap: 8px; font-size: 13px; color: #94a3b8; font-weight: 600;} .led { width: 10px; height: 10px; border-radius: 50%; display: inline-block; background: #ef4444; box-shadow: 0 0 10px rgba(239, 68, 68, 0.7); } .ledon { background: #10b981; box-shadow: 0 0 15px #10b981; } .slider-container { position: relative; width: 100%; height: 54px; background: #0b0f19; border-radius: 16px; overflow: hidden; display: flex; align-items: center; border: 1px solid rgba(255,255,255,0.05); } input[type='range'] { -webkit-appearance: none; width: 100%; height: 100%; background: transparent; outline: none; position: absolute; top: 0; left: 0; z-index: 2; cursor: pointer; } .slider-bar { position: absolute; top: 0; left: 0; height: 100%; background: #10b981; width: 100%; z-index: 1; pointer-events: none; transition: width 0.05s linear, background-color 0.3s ease; } .slider-text { position: absolute; width: 100%; display: flex; justify-content: space-between; padding: 0 18px; z-index: 3; pointer-events: none; font-size: 13px; font-weight: 800; color: #ffffff; letter-spacing: 0.5px; mix-blend-mode: difference; } input[type='range']::-webkit-slider-thumb { -webkit-appearance: none; width: 30px; height: 55px; } input[type='range']::-moz-range-thumb { width: 30px; height: 55px; background: transparent; border: none; }
</style>
</head>
<body>

<div class="header-bar">
    <h2>Helmet LED Control</h2>
    <div class="burger-menu" onclick="toggleMenu()">&#9776;</div>
</div>

<div id="sidePanel" class="side-panel">
    <span class="close-btn" onclick="toggleMenu()">&times;</span>
    <h4>Menu</h4>
    
   <div class="dropdown-box">
        <label for="frPatternSelect">Front and Rear Led Pattern</label>
        <select id="frPatternSelect" class="pattern-select" onchange="changeFRStyle(this.value)">
            <option value="1">Pattern Style 1</option>
            <option value="2">Pattern Style 2</option>
            <option value="3">Pattern Style 3</option>
            <option value="4">Pattern Style 4</option>
            <option value="5">Pattern Style 5</option>
        </select>
        <div class="speed-input-container">
            <span>Speed (ms)</span>
            <input type="number" id="frSpeedInput" min="10" max="5000" placeholder="ms">
        </div>
    </div>

    <div class="dropdown-box">
        <label for="backPatternSelect">Only for Back Led Pattern</label>
        <select id="backPatternSelect" class="pattern-select" onchange="changeBackStyle(this.value)">
            <option value="1">Pattern Style 1</option>
            <option value="2">Pattern Style 2</option>
            <option value="3">Pattern Style 3</option>
            <option value="4">Pattern Style 4</option>
            <option value="5">Pattern Style 5</option>
        </select>
        <div class="speed-input-container">
            <span>Speed (ms)</span>
            <input type="number" id="backSpeedInput" min="10" max="5000" placeholder="ms">
        </div>
        <button class="speed-submit-btn" onclick="saveSpeeds()">SET PATTERN SPEED</button>
    </div>
    
    <button id="aoBtn" class="menu-btn" onclick="tgAutoOff()">Automatic Power Off Helmet Non Activate</button>
    
    <div class="note-container">
        <div class="note-title">NOTES........</div>
        <iframe class="note-iframe" src="https://raw.githack.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Update%20Not/Upload%20.txt"></iframe>
    </div>
</div>

<div class="container">
    <div class='wrap'>
        <div class='card panel-blue'><h3>Only Front</h3><div id='b1' class='logo off' onclick='tg(1)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Front%20Led.png' alt='Front'></div></div>
        <div class='card panel-red'><h3>Back High Beam</h3><div id='b2' class='logo off' onclick='tg(2)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Back%20Led.jpg' alt='Back'></div></div>
        <div class='card panel-purple'><h3>Only Rear</h3><div id='b3' class='logo off' onclick='tg(3)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Rear%20Led.jpeg' alt='Rear'></div></div>
        
        <div class='card panel-orange'><h3>Back Low Beam</h3><div id='b7' class='logo off' onclick='tg(7)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Back%20Burst.jpg' alt='Low Beam'></div></div>
        
        <div class='card middle-yellow'>
            <div class="combo-header"><h3>BURST MODES</h3></div>
            <div class="combo-body">
                <div class="combo-item">
                    <span>Indicator Burst</span>
                    <div id='b5' class='logo off' onclick='tg(5)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/INDICATOR%20BURST.jpg' alt='Indicator'></div>
                </div>
                <div class="combo-item">
                    <span>Back Burst</span>
                    <div id='b4' class='logo off' onclick='tg(4)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/Back%20Burst.jpg' alt='Burst'></div>
                </div>
            </div>
        </div>
        
        <div class='card middle-green' style='height: auto;'>
            <div style='display: flex; justify-content: space-between; width: 100%; align-items: center;'>
                <h3>Helmet Led Del</h3>
                <div id='b6' class='logo off' onclick='tg(6)'><img src='https://raw.githubusercontent.com/pappuroy0482/Helmet-LED-Control/refs/heads/main/Helmet%20LED%20button%20photo/HELMET%20LED%20EYE.png' alt='Eye'></div>
            </div>
            
            <div class="eye-pattern-container">
                <label for="eyePatternSelect">Del Led Pattern Mode</label>
                <select id="eyePatternSelect" class="pattern-select" onchange="changeEyeStyle(this.value)">
                    <option value="1">Pattern 1: Triple Flash</option>
                    <option value="2">Pattern 2: Smooth Breathing</option>
                    <option value="3">Pattern 3: Double Blink</option>
                    <option value="4">Pattern 4: SOS Beacon</option>
                    <option value="5">Pattern 5: Fire Flicker</option>
                </select>
            </div>

            <div class="extra-btn-container">
                <button id="ex1" class="sub-btn" onclick="tgExtra(1)">Front Led</button>
                <button id="ex2" class="sub-btn" onclick="tgExtra(2)">Rear Led</button>
            </div>
            <div class='config-area'>
                <div class="input-box"><label for="maxB">Brightness</label><input type='number' id='maxB' min='1' max='100' placeholder='Max %'></div>
                <div class="input-box"><label for="fTime">On (ms)</label><input type='number' id='fTime' min='50' max='60000' placeholder='On ms'></div>
                <div class="input-box"><label for="ofTime">Off (ms)</label><input type='number' id='ofTime' min='0' max='60000' placeholder='Off ms'></div>
                <div class="set-btn-wrapper"><button onclick='saveCfg()'>SET CONFIGURATION</button></div>
            </div>
        </div>
    </div>
    
    <div class='control-panel'>
        <div class="panel-header">
            <h3>SYSTEM STATUS</h3>
            <div class="status-container">Signal: <span id='sig' class='led'></span></div>
        </div>
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

function changeBackStyle(v) { fetch('/setBackPattern?v=' + v).then(() => { setTimeout(up, 80); }); }
function changeFRStyle(v) { fetch('/setFRPattern?v=' + v).then(() => { setTimeout(up, 80); }); }
function changeEyeStyle(v) { fetch('/setEyePattern?v=' + v).then(() => { setTimeout(up, 80); }); } 

function saveSpeeds() {
    var bs = document.getElementById('backSpeedInput').value;
    var frs = document.getElementById('frSpeedInput').value;
    if(bs && frs) {
        fetch('/setPatternSpeed?bs=' + bs + '&frs=' + frs).then(() => { alert("Pattern Speeds Saved!"); up(); });
    } else { alert("Please fill both speed values!"); }
}

function saveCfg(){
    var mb = document.getElementById('maxB').value;
    var ft = document.getElementById('fTime').value;
    var oft = document.getElementById('ofTime').value;
    if(mb && ft && oft){
        fetch('/setr6?mb=' + mb + '&ft=' + ft + '&oft=' + oft).then(() => { alert("Configuration Saved!"); up(); });
    } else { alert("Please fill Brightness, On Time, and Off Time!"); }
}
function st(id,v){ document.getElementById(id).className = v ? 'logo on' : 'logo off'; }

function updateSliderColor(v) {
    var bar = document.getElementById('bar');
    if (v < 40) { bar.style.backgroundColor = '#10b981'; } 
    else if (v >= 40 && v < 75) { bar.style.backgroundColor = '#eab308'; } 
    else { bar.style.backgroundColor = '#ef4444'; }
}

function setBr(v){
    document.getElementById('brtxt').innerHTML = v + '%';
    document.getElementById('bar').style.width = v + '%';
    updateSliderColor(v);
    fetch('/brightness?v=' + v);
}
function up(){
    fetch('/status?t=' + new Date().getTime()).then(r=>r.json()).then(d=>{
        st('b1',d.r1); st('b2',d.r2); st('b3',d.r3); st('b4',d.r4); st('b5',d.r5); st('b6',d.r6); st('b7',d.lb);
        document.getElementById('ex1').className = d.b1 ? 'sub-btn active' : 'sub-btn';
        document.getElementById('ex2').className = d.b2 ? 'sub-btn active' : 'sub-btn';
        document.getElementById('sig').className = d.sig ? 'led ledon' : 'led';
        document.getElementById('br').value = d.br;
        document.getElementById('brtxt').innerHTML = d.br + '%';
        document.getElementById('bar').style.width = d.br + '%';
        updateSliderColor(d.br);
        
        var aoBtn = document.getElementById('aoBtn');
        aoBtn.className = d.ao ? 'menu-btn active' : 'menu-btn';
        aoBtn.textContent = d.ao ? "Automatic Power Off Helmet Activate" : "Automatic Power Off Helmet Non Activate";

        document.getElementById('backPatternSelect').value = d.bp;
        document.getElementById('frPatternSelect').value = d.fr;
        document.getElementById('eyePatternSelect').value = d.ep; 
        
        if(document.activeElement.id !== 'backSpeedInput') document.getElementById('backSpeedInput').value = d.bs;
        if(document.activeElement.id !== 'frSpeedInput') document.getElementById('frSpeedInput').value = d.frs;

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
    pinMode(AUTO_OFF_PIN, OUTPUT); 
    pinMode(RELAY_LOW_BEAM, OUTPUT); 

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
    server.on("/toggle7",toggleLowBeam); 
    server.on("/toggleAutoOff", toggleAutoOff); 
    server.on("/setBackPattern", setBackPatternRoute);   
    server.on("/setFRPattern", setFrontRearPatternRoute); 
    server.on("/setEyePattern", setEyePatternRoute); 
    server.on("/setPatternSpeed", setPatternSpeedConfig); 
    
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
