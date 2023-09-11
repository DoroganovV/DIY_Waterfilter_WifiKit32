#include <Arduino.h>
#include "heltec.h"
#include "WiFi.h"
//#include <ESP8266WiFi.h>
//#include <PubSubClient.h>
#include <GyverPortal.h>
#include <GyverFilters.h>
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
  int correctionFactor_1; //2280 impulses in 1 litre
  int correctionFactor_2; //2280 impulses in 1 litre
  Filter filter1;
  Filter filter2;
  Filter filter3;
  Filter filter4;
  Filter filter5;
};
struct VariableCounters {
  int LastInputImp = -1;
  int LastOutputImp = -1;
  byte flagInput = 0;   //position of rotor indicator
  byte flagOutput = 0;  //position of rotor indicator
};
struct VariableTds {
  GFilterRA tdsAnalog1;   //Running average
  GFilterRA tdsAnalog2;   //Running average
  float tdsSensor1 = 0;   //value TDS of sensor 1
  float tdsSensor2 = 0;   //value TDS of sensor 2
  float temperature = 25; //temperature for correction (not used)
};

const String version = "2.0.0";
const byte PIN_COUNTER_1 = 21;  //PIN of first counter
const byte PIN_COUNTER_2 = 22;  //PIN of second counter
const byte PIN_TDS_1 = 19;      //PIN of first TDS sensor
const byte PIN_TDS_2 = 23;      //PIN of second TDS sensor
const long PERIOD = 250;        //periud of update display
const char rotor[4] = {'/', '-', '\\', '|'};//imitation actions of rotor
const int defaultCorrectionFactor = 2280;   //default impulses in 1 litre
const double analogFactor = 3.3 / 4095.0;   //conversing analog value = analogMaxVoltage / analogDiscrete

MemoryData memoryData;            //strucure for saving
VariableCounters variableCounters;//strucure of counter
VariableTds variableTds;          //strucure of TDS sensors
GyverPortal ui(&LittleFS);  //Web UI
unsigned long timer;        //Timer for updating
volatile int InputImp = 0;  //Impulses count of counter 1 by interrupt
volatile int OutputImp = 0; //Impulses count of counter 2 by interrupt
byte mode = 0;

WiFiClient espClient;       //WiFi

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

void CheckSensors()
{
  if (variableCounters.LastInputImp != InputImp) {
    if (InputImp > memoryData.correctionFactor_1) {
      int l = InputImp / memoryData.correctionFactor_1;
      InputImp -= l * memoryData.correctionFactor_1;
      UpdateFilters(true, l);
      Serial.print("Counter 1: spent "); Serial.print(l); Serial.println(" liter(s)");
    }
    variableCounters.flagInput = (variableCounters.flagInput + 1) % 4;
  }

  if (variableCounters.LastOutputImp != OutputImp) {
    if (OutputImp > memoryData.correctionFactor_2) {
      int l = OutputImp / memoryData.correctionFactor_2;
      OutputImp -= l * memoryData.correctionFactor_2;
      UpdateFilters(false, l);
      Serial.print("Counter 2: spent "); Serial.print(l); Serial.println(" liter(s)");
    }
    variableCounters.flagOutput = (variableCounters.flagOutput + 1) % 4;
  }

  float compensationCoefficient = 1.0 + 0.02 * (variableTds.temperature - 25.0);
  {
    int averageVoltage = variableTds.tdsAnalog1.filteredTime(analogRead(PIN_TDS_1)) * analogFactor;
    float compensationVoltage = averageVoltage / compensationCoefficient;
    variableTds.tdsSensor1 = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;
  }

  {
    int averageVoltage = variableTds.tdsAnalog1.filteredTime(analogRead(PIN_TDS_2)) * analogFactor;
    float compensationVoltage = averageVoltage / compensationCoefficient;
    variableTds.tdsSensor2 = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;
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
      M_BOX(GP.LABEL("1. Resource balance, L"); GP.LABEL(String(memoryData.filter1.remainingResource)); GP.LED("ledf1rb", memoryData.filter1.remainingResource > 1000 ? 1 : 0););
      GP.HR();
      M_BOX(GP.LABEL("2. Resource balance, L"); GP.LABEL(String(memoryData.filter2.remainingResource)); GP.LED("ledf2rb", memoryData.filter2.remainingResource > 1000 ? 1 : 0););
      GP.HR();
      M_BOX(GP.LABEL("3. Resource balance, L"); GP.LABEL(String(memoryData.filter3.remainingResource)); GP.LED("ledf3rb", memoryData.filter3.remainingResource > 1000 ? 1 : 0););
      GP.HR();
      M_BOX(GP.LABEL("4. Resource balance, L"); GP.LABEL(String(memoryData.filter4.remainingResource)); GP.LED("ledf4rb", memoryData.filter4.remainingResource > 1000 ? 1 : 0););
      GP.HR();
      M_BOX(GP.LABEL("5. Resource balance, L"); GP.LABEL(String(memoryData.filter5.remainingResource)); GP.LED("ledf5rb", memoryData.filter5.remainingResource > 1000 ? 1 : 0););
      GP.HR();
      M_BOX(GP.LABEL("TDS before filter, ppm"); GP.LABEL(String(variableTds.tdsSensor1)); GP.LED("ledTds1", variableTds.tdsSensor1 < 50 ? 1 : 0););
      GP.HR();
      M_BOX(GP.LABEL("TDS after filter, ppm"); GP.LABEL(String(variableTds.tdsSensor2)); GP.LED("ledTds1", variableTds.tdsSensor2 < 50 ? 1 : 0););
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
      M_BOX(GP.LABEL("1. Resource balance"); GP.NUMBER("f1rb", "50000", memoryData.filter1.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("2. First counter"); GP.CHECK("f2c", memoryData.filter2.isFirstCounter););
      M_BOX(GP.LABEL("2. Resource balance"); GP.NUMBER("f2rb", "10000", memoryData.filter2.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("3. First counter"); GP.CHECK("f3c", memoryData.filter3.isFirstCounter););
      M_BOX(GP.LABEL("3. Resource balance"); GP.NUMBER("f3rb", "50000", memoryData.filter3.remainingResource););
      GP.HR();
      M_BOX(GP.LABEL("4. First counter"); GP.CHECK("f4c", memoryData.filter4.isFirstCounter););
      M_BOX(GP.LABEL("4. Resource balance"); GP.NUMBER("f4rb", "150000", memoryData.filter4.remainingResource););
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
    GP.BUTTON_LINK("https://github.com/GyverLibs/GyverFilters", "Running average GyverFilters");
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
  pinMode(PIN_TDS_1, INPUT);
  pinMode(PIN_TDS_2, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(PIN_COUNTER_1), Counter_1_Tick, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_COUNTER_2), Counter_2_Tick, RISING);

  CheckSensors();
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
    memoryData.filter1.remainingResource = 50000;
    memoryData.filter2.isFirstCounter = true;
    memoryData.filter2.remainingResource = 10000;
    memoryData.filter3.isFirstCounter = true;
    memoryData.filter3.remainingResource = 50000;
    memoryData.filter4.isFirstCounter = true;
    memoryData.filter4.remainingResource = 150000;
    memoryData.filter5.isFirstCounter = false;
    memoryData.filter5.remainingResource = 10000;
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
void AnalogSetup() {
  variableTds.tdsAnalog1.setCoef(0.2);
  variableTds.tdsAnalog1.setStep(10);
  variableTds.tdsAnalog2.setCoef(0.2);
  variableTds.tdsAnalog2.setStep(10);
}

void UpdateDislay()
{
  //if (variableCounters.LastInputImp != InputImp || variableCounters.LastOutputImp != OutputImp) 
  {
    Heltec.display -> clear();
    Heltec.display -> drawString(0,  0, String(rotor[variableCounters.flagInput]));
    Heltec.display -> drawString(60, 0, String(rotor[variableCounters.flagOutput]));
    
    Heltec.display -> drawString(0, 10, "Debug mode");
    Heltec.display -> drawString(0, 20, String(InputImp));
    Heltec.display -> drawString(60, 20,  String(OutputImp));
    Heltec.display -> drawString(0, 30, String(variableTds.tdsSensor1));
    Heltec.display -> drawString(60, 30,  String(variableTds.tdsSensor2));
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
  AnalogSetup();
  UpdateDislay();
  timer = millis();
}

void loop() {
  if (millis() < timer)
    timer = millis();
  if (millis() - timer >= PERIOD) {
    timer = millis();
    if (mode > 0) {
      CheckSensors();
      UpdateDislay();
    }
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  variableCounters.LastInputImp = InputImp;
  variableCounters.LastOutputImp = OutputImp;
  ui.tick();
}