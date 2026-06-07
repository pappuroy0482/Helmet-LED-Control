/*************************************************
                ESP8266 RECEIVER
         6 Channel Relay Receiver
      Auto OFF if Signal Lost
*************************************************/

#include <ESP8266WiFi.h>
#include <espnow.h>

/************* Relay Pins *************/
#define RELAY1 D1
#define RELAY2 D2
#define RELAY3 D7
#define RELAY4 D0
#define RELAY5 D5
#define RELAY6 D6

/************* Data Structure *************/
typedef struct struct_message {
  bool ch1;
  bool ch2;
  bool ch3;
  bool ch4;
  bool ch5;
  bool ch6;
} struct_message;

struct_message data;

/************* Timeout *************/
unsigned long lastReceiveTime = 0;
const unsigned long timeout = 1000; // 1 Second

/************* All Relay OFF *************/
void allRelayOff() {

  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, LOW);
  digitalWrite(RELAY5, LOW);
  digitalWrite(RELAY6, LOW);
}

/************* Receive Callback *************/
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {

  memcpy(&data, incomingData, sizeof(data));

  // Last Data Receive Time Update
  lastReceiveTime = millis();

  // Relay Control
  digitalWrite(RELAY1, data.ch1);
  digitalWrite(RELAY2, data.ch2);
  digitalWrite(RELAY3, data.ch3);
  digitalWrite(RELAY4, data.ch4);
  digitalWrite(RELAY5, data.ch5);
  digitalWrite(RELAY6, data.ch6);

  Serial.println("Data Received");
}

/************* Setup *************/
void setup() {

  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  pinMode(RELAY5, OUTPUT);
  pinMode(RELAY6, OUTPUT);

  // সব Relay শুরুতে OFF
  allRelayOff();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  /************* Receiver MAC *************/
  Serial.print("Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());

  /************* ESP-NOW Init *************/
  if (esp_now_init() != 0) {

    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Receiver Ready");
}

/************* Loop *************/
void loop() {

  // যদি নির্দিষ্ট সময় Data না আসে
  // তাহলে সব Relay OFF

  if (millis() - lastReceiveTime > timeout) {

    allRelayOff();
  }
}