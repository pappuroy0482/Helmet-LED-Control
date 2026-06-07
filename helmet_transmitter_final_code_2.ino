/*************************************************
              ESP8266 TRANSMITTER
         6 Channel ESP-NOW Sender
*************************************************/

#include <ESP8266WiFi.h>
#include <espnow.h>

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

/************* Receiver MAC Address *************/
uint8_t receiverMAC[] = {0xC8, 0xC9, 0xA3, 0x5E, 0xDB, 0x6D};

/************* Button Pins *************/
#define BTN1 D1
#define BTN2 D2
#define BTN3 D7
#define BTN4 D0
#define BTN5 D5
#define BTN6 D6

/************* Send Callback *************/
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {

  Serial.print("Send Status: ");

  if (sendStatus == 0) {
    Serial.println("Success");
  } else {
    Serial.println("Fail");
  }
}

/************* Setup *************/
void setup() {

  Serial.begin(115200);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);
  pinMode(BTN5, INPUT_PULLUP);
  pinMode(BTN6, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("Transmitter MAC: ");
  Serial.println(WiFi.macAddress());

  /************* ESP-NOW Init *************/
  if (esp_now_init() != 0) {

    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  esp_now_register_send_cb(OnDataSent);

  /************* Add Receiver *************/
  esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  Serial.println("Transmitter Ready");
}

/************* Loop *************/
void loop() {

  /************* Read Buttons *************/
  data.ch1 = !digitalRead(BTN1);
  data.ch2 = !digitalRead(BTN2);

  // D7 Button
  bool d7State = !digitalRead(BTN3);

  // D5 এবং D6 Button
  bool d5State = !digitalRead(BTN5);
  bool d6State = !digitalRead(BTN6);

  // Main Channel Assign
  data.ch3 = d7State;
  data.ch4 = !digitalRead(BTN4);
  data.ch5 = d5State;
  data.ch6 = d6State;

  /*************************************************
      Logic:
      যদি D5 অথবা D6 ON হয়,
      তাহলে D7 (ch3) ও ON হবে
  *************************************************/
  if (d5State || d6State) {
    data.ch3 = true;
  }

  /************* Send Data *************/
  esp_now_send(receiverMAC, (uint8_t *) &data, sizeof(data));

  delay(50);
}