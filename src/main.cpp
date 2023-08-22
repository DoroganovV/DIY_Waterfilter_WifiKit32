#include <Arduino.h>
#include "heltec.h"
#include "WiFi.h"
//#include <ESP8266WiFi.h>
//#include <PubSubClient.h>
#include <GyverPortal.h>
#include <EEPROM.h>
#include <LittleFS.h>

#define BAND    868E6  //you can set band here directly,e.g. 868E6,915E6

struct Filter {
  bool isFirstCounter;
  int remainingResource;
  //int defaultResource; //for hardware reset
  //date dateNextReplace; //for notify by date
};
struct MemoryData {
  char ssid[20];
  char pass[20];
  int correctionFactor_1; //2280
  int correctionFactor_2; //2280
  Filter filter1;
  Filter filter2;
  Filter filter3;
  Filter filter4;
  Filter filter5;
};
struct Variables {
  int LastInputImp = -1;
  int LastOutputImp = -1;
  byte flagInput = 0;
  byte flagOutput = 0;
};

const String version = "2.0.0";
const byte PIN_COUNTER_1 = 22; //mini D1
const byte PIN_COUNTER_2 = 21; //mini D2
const long PERIOD = 1000;
const char rotor[4] = {'/', '-', '\\', '|'};
const int defaultCorrectionFactor = 2280;

MemoryData memoryData;
Variables variables;
GyverPortal ui(&LittleFS);
unsigned long timer;
volatile int InputImp = 0;
volatile int OutputImp = 0;
byte mode = 0;

WiFiClient espClient;

void SaveData(bool force) {
  if (mode > 0  || force) {
    EEPROM.put(0, memoryData);
    EEPROM.commit();
  }
}

void UpdateFilters(bool counter, int liters)
{
  bool isNeedSave = false;
  if (memoryData.filter1.isFirstCounter == counter && memoryData.filter1.remainingResource > 0) {
    memoryData.filter1.remainingResource-=liters;
    isNeedSave = true;
  }
  if (memoryData.filter2.isFirstCounter == counter && memoryData.filter2.remainingResource > 0) {
    memoryData.filter2.remainingResource-=liters;
    isNeedSave = true;
  }
  if (memoryData.filter3.isFirstCounter == counter && memoryData.filter3.remainingResource > 0) {
    memoryData.filter3.remainingResource-=liters;
    isNeedSave = true;
  }
  if (memoryData.filter4.isFirstCounter == counter && memoryData.filter4.remainingResource > 0) {
    memoryData.filter4.remainingResource-=liters;
    isNeedSave = true;
  }
  if (memoryData.filter5.isFirstCounter == counter && memoryData.filter5.remainingResource > 0) {
    memoryData.filter5.remainingResource-=liters;
    isNeedSave = true;
  }

  if (isNeedSave) SaveData(false);
}

void CheckLiters()
{
  if (variables.LastInputImp != InputImp) {
    if (InputImp > memoryData.correctionFactor_1) {
      int l = InputImp / memoryData.correctionFactor_1;
      InputImp -= l * memoryData.correctionFactor_1;
      UpdateFilters(true, l);
      Serial.print("Counter 1: spent "); Serial.print(l); Serial.println(" liter(s)");
    }
    variables.flagInput = (variables.flagInput + 1) % 4;
  }

  if (variables.LastOutputImp != OutputImp) {
    if (OutputImp > memoryData.correctionFactor_2) {
      int l = OutputImp / memoryData.correctionFactor_2;
      OutputImp -= l * memoryData.correctionFactor_2;
      UpdateFilters(false, l);
      Serial.print("Counter 2: spent "); Serial.print(l); Serial.println(" liter(s)");
    }
    variables.flagOutput = (variables.flagOutput + 1) % 4;
  }
}

void Build() {
  GP.BUILD_BEGIN();
  GP.THEME(GP_DARK);
  GP.UI_BEGIN("Smart counter water filter", "/,/wifi,/settings,/about", "Control,WiFi,Settings,About");
  GP.PAGE_TITLE("Smart counter water filter " + version);
  GP.ONLINE_CHECK();
  if (ui.uri("/")) {
    M_BLOCK_TAB("FilterValue",
      M_BOX(GP.LABEL("1. Resource balance"); GP.LABEL(String(memoryData.filter1.remainingResource)););
      GP.HR();
      M_BOX(GP.LABEL("2. Resource balance"); GP.LABEL(String(memoryData.filter2.remainingResource)););
      GP.HR();
      M_BOX(GP.LABEL("3. Resource balance"); GP.LABEL(String(memoryData.filter3.remainingResource)););
      GP.HR();
      M_BOX(GP.LABEL("4. Resource balance"); GP.LABEL(String(memoryData.filter4.remainingResource)););
      GP.HR();
      M_BOX(GP.LABEL("5. Resource balance"); GP.LABEL(String(memoryData.filter5.remainingResource)););
    );
  } else if (ui.uri("/wifi")) {
    GP.FORM_BEGIN("/wifi");
    M_BLOCK_TAB("WiFi",
      M_BOX(GP.LABEL("SSID"); GP.TEXT("lg", "SSID", memoryData.ssid););
      M_BOX(GP.LABEL("Password"); GP.PASS("ps", "Password", memoryData.pass););
      GP.SUBMIT("Save");
    );
    GP.FORM_END();
  } else if (ui.uri("/settings")) {
    GP.FORM_BEGIN("/settings");
    M_BLOCK_TAB("FILTERS",
      M_BOX(GP.LABEL("1. First counter"); GP.CHECK("f1c", memoryData.filter1.isFirstCounter););
      M_BOX(GP.LABEL("1. Resource balance"); GP.NUMBER("f1rb", "10000", memoryData.filter1.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("2. First counter"); GP.CHECK("f2c", memoryData.filter2.isFirstCounter););
      M_BOX(GP.LABEL("2. Resource balance"); GP.NUMBER("f2rb", "10000", memoryData.filter2.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("3. First counter"); GP.CHECK("f3c", memoryData.filter3.isFirstCounter););
      M_BOX(GP.LABEL("3. Resource balance"); GP.NUMBER("f3rb", "10000", memoryData.filter3.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("4. First counter"); GP.CHECK("f4c", memoryData.filter4.isFirstCounter););
      M_BOX(GP.LABEL("4. Resource balance"); GP.NUMBER("f4rb", "10000", memoryData.filter4.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("5. First counter"); GP.CHECK("f5c", memoryData.filter5.isFirstCounter););
      M_BOX(GP.LABEL("5. Resource balance"); GP.NUMBER("f5rb", "10000", memoryData.filter5.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("Correction factor 1"); GP.NUMBER("cf1", String(defaultCorrectionFactor), memoryData.correctionFactor_1););
      M_BOX(GP.LABEL("Correction factor 2"); GP.NUMBER("cf2", String(defaultCorrectionFactor), memoryData.correctionFactor_2););
      GP.SUBMIT("Save");
    );
    GP.FORM_END();
  } else if (ui.uri("/about")) {
    GP.TITLE("Smart counter water filter " + version); GP.BREAK();
    GP.SPAN("Smart counter water filter is project for automatisation water filter."); GP.BREAK();
    GP.HR();    
    GP.BUTTON_LINK("https://heltec.org/project/wifi-kit-32/", "Heltec wifi kit 32");
    GP.BUTTON_LINK("https://github.com/GyverLibs/GyverPortal", "UI GyverPortal");
    GP.BUTTON_LINK("https://github.com/DoroganovV", "Autor Vitaly Doroganov");
    GP.HR();
    GP.OTA_FIRMWARE("Upload firmware");
    GP.BUTTON("reset", "Reset settings to default");
    GP.HR();
    GP.SYSTEM_INFO(version);
  } 
  GP.BUILD_END();
}

void ICACHE_RAM_ATTR Counter_1_Tick();
void ICACHE_RAM_ATTR Counter_2_Tick();

void Counter_1_Tick() {
  InputImp++;
}
void Counter_2_Tick() {
  OutputImp++;
}

void PinSetup() {
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(PIN_COUNTER_1, INPUT_PULLUP);
  pinMode(PIN_COUNTER_2, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(PIN_COUNTER_1), Counter_1_Tick, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_COUNTER_2), Counter_2_Tick, RISING);

  CheckLiters();
}

void Action(GyverPortal& p) {
  if (p.form("/wifi")) {
    Serial.println("save wifi");
    p.copyStr("lg", memoryData.ssid);
    p.copyStr("ps", memoryData.pass);
    SaveData(true);
    delay(5000);
    ESP.restart();
  }

  if (p.form("/settings")) {
    Serial.println("save settings");
    p.copyBool("f1c", memoryData.filter1.isFirstCounter);
    p.copyInt("f1rb", memoryData.filter1.remainingResource);
    p.copyBool("f2c", memoryData.filter2.isFirstCounter);
    p.copyInt("f2rb", memoryData.filter2.remainingResource);
    p.copyBool("f3c", memoryData.filter3.isFirstCounter);
    p.copyInt("f3rb", memoryData.filter3.remainingResource);
    p.copyBool("f4c", memoryData.filter4.isFirstCounter);
    p.copyInt("f4rb", memoryData.filter4.remainingResource);
    p.copyBool("f5c", memoryData.filter5.isFirstCounter);
    p.copyInt("f5rb", memoryData.filter5.remainingResource);
    SaveData(true);
    delay(5000);
    ESP.restart();
  }
  if (ui.click("reset")) {
    strcpy(memoryData.ssid, "");
    strcpy(memoryData.pass, "");
    memoryData.filter1.isFirstCounter = true;
    memoryData.filter1.remainingResource = 10000;
    memoryData.filter2.isFirstCounter = true;
    memoryData.filter2.remainingResource = 10000;
    memoryData.filter3.isFirstCounter = true;
    memoryData.filter3.remainingResource = 10000;
    memoryData.filter4.isFirstCounter = true;
    memoryData.filter4.remainingResource = 30000;
    memoryData.filter5.isFirstCounter = false;
    memoryData.filter5.remainingResource = 5000;
    memoryData.correctionFactor_1 = defaultCorrectionFactor;
    memoryData.correctionFactor_2 = defaultCorrectionFactor;
    SaveData(true);
    delay(5000);
    ESP.restart();
  }
  if (ui.update()) {
    if (ui.update("f1rb")) ui.answer(memoryData.filter1.remainingResource);
    if (ui.update("f2rb")) ui.answer(memoryData.filter2.remainingResource);
    if (ui.update("f3rb")) ui.answer(memoryData.filter3.remainingResource);
    if (ui.update("f4rb")) ui.answer(memoryData.filter4.remainingResource);
    if (ui.update("f5rb")) ui.answer(memoryData.filter5.remainingResource);
  }
}

void PortalSetup() {
  if (!LittleFS.begin()) Serial.println("FS Error");
  Serial.println("Portal start");
  ui.attachBuild(Build);
  ui.start("");
  ui.attach(Action);
  //ui.enableOTA();
  //ui.downloadAuto(true);
}

void WiFiSetup() {
  delay(10);
  mode = 0;
  Heltec.display -> clear();
  if (strlen(memoryData.ssid) != 0 || strlen(memoryData.pass) != 0) {
    Heltec.display -> drawString(0, 0, "Connecting to " + String(memoryData.ssid));
    WiFi.mode(WIFI_STA);
    WiFi.begin(memoryData.ssid, memoryData.pass);
    for (int i = 0; i < 10; i++) {
      delay(1000);
      Heltec.display -> drawString(i * 10, 10, ".");
      Heltec.display -> display();
      if (WiFi.status() == WL_CONNECTED) {
        i = 10;
        mode = 1;
        Heltec.display -> drawString(0, 20, "Connected!");
        Heltec.display -> drawString(0, 30, "IP: " + String(WiFi.localIP()));
        Heltec.display -> display();
      }
    }
  }

  if (mode == 0) {
    randomSeed(micros());
    WiFi.mode(WIFI_AP);
    IPAddress IP = WiFi.softAPIP();
    Heltec.display -> drawString(0, 20, "Hotspot mode.");
    Heltec.display -> drawString(0, 30, "IP: " + IP.toString());
    Heltec.display -> display();
  }
  delay(1000);
}

void UpdateDislay()
{
  if (variables.LastInputImp != InputImp || variables.LastOutputImp != OutputImp) 
  {
    Heltec.display -> clear();
    Heltec.display -> drawString(0,  0, String(rotor[variables.flagInput]));
    Heltec.display -> drawString(60, 0, String(rotor[variables.flagOutput]));
    
    Heltec.display -> drawString(0, 10, "Debug mode");
    Heltec.display -> drawString(0, 20, String(InputImp));
    Heltec.display -> drawString(0, 30,  String(OutputImp));
    Heltec.display -> display();
  }
}

void setup() {
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, false /*Serial Enable*/, false /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);
  delay(2000);
  Serial.begin(115200);
  Serial.println();

  PinSetup();
  EEPROM.begin(300);
  EEPROM.get(0, memoryData);

  WiFiSetup();
  PortalSetup();
  UpdateDislay();
  timer = millis();
}

void loop() {
  if (millis() < timer)
    timer = millis();
    
  if (millis() - timer >= PERIOD) {
    timer = millis();
    if (mode > 0) {
      CheckLiters();
      UpdateDislay();
    }
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  variables.LastInputImp = InputImp;
  variables.LastOutputImp = OutputImp;
  ui.tick();
}