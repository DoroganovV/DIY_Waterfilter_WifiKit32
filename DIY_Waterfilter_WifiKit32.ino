#include "Arduino.h"
#include "heltec.h"
#include "WiFi.h"
#include "images.h"
//#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define BAND    868E6  //you can set band here directly,e.g. 868E6,915E6

unsigned long timer;
const long PERIOD = 1000;

const char* WIFI_SSID = "{YOUR_SSED}";
const char* WIFI_PASSWORD = "{YOUR_PASSWORD}";
const char* MQTT_SERVER = "{YOUR_MQTT_SERVER}"; //Сервер MQTT

const byte PIN_COUNTER_1 = 21; //mini D1
const byte PIN_COUNTER_2 = 22; //mini D2
const int ACCURACY_COUNTER_1 = 2280;
const int ACCURACY_COUNTER_2 = 2280;

const char* TOPIC_INPUT    = "/devices/waterfilter/controls/Input";
const char* TOPIC_OUTPUT   = "/devices/waterfilter/controls/Output";
const char* TOPIC_FILTER_1 = "/devices/waterfilter/controls/Filter1";
const char* TOPIC_FILTER_2 = "/devices/waterfilter/controls/Filter2";
const char* TOPIC_FILTER_3 = "/devices/waterfilter/controls/Filter3";
const char* TOPIC_FILTER_4 = "/devices/waterfilter/controls/Filter4";
const char* TOPIC_FILTER_5 = "/devices/waterfilter/controls/Filter5";

const char* TOPIC_INPUT_ON    = "/devices/waterfilter/controls/Input/on";
const char* TOPIC_OUTPUT_ON   = "/devices/waterfilter/controls/Output/on";
const char* TOPIC_FILTER_1_ON = "/devices/waterfilter/controls/Filter1/on";
const char* TOPIC_FILTER_2_ON = "/devices/waterfilter/controls/Filter2/on";
const char* TOPIC_FILTER_3_ON = "/devices/waterfilter/controls/Filter3/on";
const char* TOPIC_FILTER_4_ON = "/devices/waterfilter/controls/Filter4/on";
const char* TOPIC_FILTER_5_ON = "/devices/waterfilter/controls/Filter5/on";

int InputL = -1;
int OutputL = -1;
volatile int InputImp = -1;
volatile int OutputImp = -1;

int balanceFilter1 = -1;
int balanceFilter2 = -1;
int balanceFilter3 = -1;
int balanceFilter4 = -1;
int balanceFilter5 = -1;

int maxBalanceFilter1 = 150;
int maxBalanceFilter2 = 130;
int maxBalanceFilter3 = 130;
int maxBalanceFilter4 = 50;
int maxBalanceFilter5 = 50;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Heltec.display -> drawString(0, 0, "WiFi: ");
    Heltec.display -> display();
  }

  randomSeed(micros());
  if(WiFi.status() == WL_CONNECTED)
    Heltec.display -> drawString(0, 0, "WiFi: OK");
  else
    Heltec.display -> drawString(0, 0, "WiFi: Failed");
  Heltec.display -> display();
}

void setup_MQTT() {
  while (!client.connected()) {
    Heltec.display -> drawString(0, 10, "MQTT: ");
    Heltec.display -> display();
    
    String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Heltec.display -> drawString(0, 10, "MQTT: OK");
      Heltec.display -> display();
    } else {
      Heltec.display -> drawString(0, 10, "MQTT: Failed");
      Heltec.display -> display();
      delay(5000);
    }
  }
  
  delay(2000);
  Heltec.display -> clear();
  Heltec.display -> display();
  
  if (InputL == -1)  client.subscribe(TOPIC_INPUT);
  if (OutputL == -1) client.subscribe(TOPIC_OUTPUT);
  if (balanceFilter1 == -1) client.subscribe(TOPIC_FILTER_1);
  if (balanceFilter2 == -1) client.subscribe(TOPIC_FILTER_2);
  if (balanceFilter3 == -1) client.subscribe(TOPIC_FILTER_3);
  if (balanceFilter4 == -1) client.subscribe(TOPIC_FILTER_4);
  if (balanceFilter5 == -1) client.subscribe(TOPIC_FILTER_5);
}

void ICACHE_RAM_ATTR Counter_1_Tick();
void ICACHE_RAM_ATTR Counter_2_Tick();

void Counter_1_Tick() {
  InputImp++;
}
void Counter_2_Tick() {
  OutputImp++;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String strTopic = String(topic);
  String strPayload = "";
  for (int i = 0; i < length; i++)
    strPayload += (char)payload[i];

  if (strTopic == TOPIC_INPUT) {
    InputL = strPayload.toInt();
    client.unsubscribe(TOPIC_INPUT);
  } else if (strTopic == TOPIC_OUTPUT) {
    OutputL = strPayload.toInt();
    client.unsubscribe(TOPIC_OUTPUT);
  } else if (strTopic == TOPIC_FILTER_1 && strPayload.toInt() > balanceFilter1) {
    balanceFilter1 = strPayload.toInt();
    //client.unsubscribe(TOPIC_FILTER_1);
  } else if (strTopic == TOPIC_FILTER_2 && strPayload.toInt() > balanceFilter2) {
    balanceFilter2 = strPayload.toInt();
    //client.unsubscribe(TOPIC_FILTER_2);
  } else if (strTopic == TOPIC_FILTER_3 && strPayload.toInt() > balanceFilter3) {
    balanceFilter3 = strPayload.toInt();
    //client.unsubscribe(TOPIC_FILTER_3);
  } else if (strTopic == TOPIC_FILTER_4 && strPayload.toInt() > balanceFilter4) {
    balanceFilter4 = strPayload.toInt();
    //client.unsubscribe(TOPIC_FILTER_4);
  } else if (strTopic == TOPIC_FILTER_5 && strPayload.toInt() > balanceFilter5) {
    balanceFilter5 = strPayload.toInt();
    //client.unsubscribe(TOPIC_FILTER_5);
  }
}

void UpdateValue()
{
  if (InputImp > ACCURACY_COUNTER_1) {
    int L = InputImp / ACCURACY_COUNTER_1;
    InputImp -= L * ACCURACY_COUNTER_1;
    InputL += L;
    balanceFilter1 -= L;
    balanceFilter2 -= L;
    balanceFilter3 -= L;
    client.publish(TOPIC_INPUT_ON, (char*)String(InputL).c_str());
    client.publish(TOPIC_FILTER_1_ON, (char*)String(balanceFilter1).c_str());
    client.publish(TOPIC_FILTER_2_ON, (char*)String(balanceFilter2).c_str());
    client.publish(TOPIC_FILTER_3_ON, (char*)String(balanceFilter3).c_str());
  }
  if (OutputImp > ACCURACY_COUNTER_2) {
    int L = OutputImp / ACCURACY_COUNTER_2;
    OutputImp -= L * ACCURACY_COUNTER_2;
    OutputL += L;
    balanceFilter4 -= L;
    balanceFilter5 -= L;
    client.publish(TOPIC_OUTPUT_ON, (char*)String(OutputL).c_str());
    client.publish(TOPIC_FILTER_4_ON, (char*)String(balanceFilter4).c_str());
    client.publish(TOPIC_FILTER_5_ON, (char*)String(balanceFilter5).c_str());
  }
  
  Heltec.display -> clear();
  Heltec.display -> drawString(0,  0, String(InputImp)  + " (" + String(InputL)  + " l)");
  Heltec.display -> drawString(60, 0, String(OutputImp) + " (" + String(OutputL) + " l)");
  
  Heltec.display -> drawProgressBar(50, 11, 75, 8, (balanceFilter1 / maxBalanceFilter1));
  Heltec.display -> drawString(0, 10, "1) " + String(balanceFilter1));

  Heltec.display -> drawProgressBar(50, 21, 75, 8, (balanceFilter2 / maxBalanceFilter2));
  Heltec.display -> drawString(0, 20, "2) " + String(balanceFilter2));

  Heltec.display -> drawProgressBar(50, 31, 75, 8, (balanceFilter3 / maxBalanceFilter3));
  Heltec.display -> drawString(0, 30, "3) " + String(balanceFilter3));

  Heltec.display -> drawProgressBar(50, 41, 75, 8, (balanceFilter4 / maxBalanceFilter4));
  Heltec.display -> drawString(0, 40, "4) " + String(balanceFilter4));

  Heltec.display -> drawProgressBar(50, 51, 75, 8, (balanceFilter5 / maxBalanceFilter5));
  Heltec.display -> drawString(0, 50, "5) " + String(balanceFilter5));

  Heltec.display -> display();
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void setup() {
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, false /*Serial Enable*/, false /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);
  Heltec.display -> clear();
  Heltec.display -> display();
  
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(PIN_COUNTER_1, INPUT_PULLUP);
  pinMode(PIN_COUNTER_2, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(PIN_COUNTER_1), Counter_1_Tick, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_COUNTER_2), Counter_2_Tick, RISING);
  
  setup_wifi();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  timer = millis();
}

void loop() {
  if (millis() - timer >= PERIOD) {
    timer = millis();
    if (!client.connected()) {
      setup_MQTT();
      delay(500);
    } else {
      UpdateValue();
    }
  }
  client.loop();
}
