#include <RadioLib.h>
#include <Wire.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
// lora define
#define CS 18
#define DIO0 26
#define reset 4
#define DIO1 33
#define SCK 5
#define MISO 19
#define MOSI 27
// gps define
#define gps_rx 34
#define gps_tx 12
#define gps_baud 9600

#define SDA 21
#define SCL 22
#define led 4
#define AXP_adr 0x34
#define pw_pin 32

#define frq 905.0
#define bandw 250.0
#define spread 7
#define code_rate 5
#define sync_sos 0x34
#define sync_gnd 0xAB

#define frame 1000UL
#define rx_ms 850UL
#define sos_ms 10000UL
#define ack_ms 900UL

#define bms_ms 200
#define led_ms 200

SX1276 radio = new Module(CS, DIO0, reset, DIO1);
Adafruit_BME280 bme;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

//definning states
bool bmeReady = false;
float base_pres = 1013.25;
unsigned long ledOnAt = 0;
bool ledOn = false;
unsigned long pac = 0;

//DIO0 interrrupt when a packet arrive
volatile bool receive = false;

float snapTemp = -99;
float snapHum = -99;
float snapPres = -99;
float snapAlt = -99;
unsigned long bme_update = 0;

// sos cache
bool sosReceive = false;
bool sosFresh = false;
unsigned long sos_heart = 0;
unsigned long ack = 0;
String sosBPM = "n";
String sosLat = "n";
String sosLon = "n";
String sosTemp = "n";
String sosHum = "n";

//Axp2101
void axp_w(uint8_t reg, uint8_t val){
  Wire.beginTransmission(AXP_adr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
uint8_t axp_r(uint8_t reg){
  Wire.beginTransmission(AXP_adr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(AXP_adr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

//led flash
void led_flash(){digitalWrite(led, HIGH); ledOn=true; ledOnAt=millis();}

void updateLED(){
  if(ledOn && millis() - ledOnAt >= led_ms){
    digitalWrite(led, LOW); ledOn = false;
  }
}

//bme update
void bms_snap(){
  if (!bmeReady) return;
  if (millis() - bme_update < bms_ms) return;
  bme_update = millis();
  snapTemp = bme.readTemperature();
  snapHum  = bme.readHumidity();
  snapPres = bme.readPressure()/100.0F;
  snapAlt  = bme.readAltitude(base_pres);
}

void sos_update(){
  if (sosFresh && (millis() - sos_heart > sos_ms)){
    sosFresh = false;
    sosBPM = "n"; sosLat = "n"; sosLon = "n"; sosTemp = "n"; sosHum = "n";
    Serial.println(F("sos data clear"));
  }
}

void sos_cashe(const String& pkt){
  //bpm valid when finger on sensor
  int finIdx = pkt.indexOf("finger:");
  if (finIdx != -1 && pkt.substring(finIdx + 4, finIdx + 5) == "1"){
    int idx = pkt.indexOf("BPM:");
    if (idx != -1){
      int end = pkt.indexOf(",", idx);
      if (end < 0) end = pkt.length();
      sosBPM = pkt.substring(idx + 4, end);
    }else sosBPM = "n";
  } 
  else {
    sosBPM = "n";
  }

  //gps from radio device
  int gpsIdx = pkt.indexOf("GPS:");
  if (gpsIdx != -1 && pkt.substring(gpsIdx + 4, gpsIdx + 5) == "1"){
    int latIdx = pkt.indexOf("Lat:");
    if (latIdx != -1){
      int end = pkt.indexOf(",", latIdx);
      if (end < 0) end = pkt.length();
      sosLat = pkt.substring(latIdx + 4, end);
    }
    int lngIdx = pkt.indexOf("lon:");
    if (lngIdx != -1){
      int end = pkt.indexOf(",", lngIdx);
      if (end < 0) end = pkt.length();
      sosLon = pkt.substring(lngIdx + 4, end);
    }
  } 
  else{
    sosLat = "n";
    sosLon = "n";
  }

  //env data cahse from radio device
  int tIdx = pkt.indexOf(",T:");
  if (tIdx != -1){
    int end = pkt.indexOf(",", tIdx + 1);
    if (end < 0) end = pkt.length();
    sosTemp = pkt.substring(tIdx + 3, end);
  }else sosTemp = "n";

  int hIdx = pkt.indexOf(",H:");
  if (hIdx != -1){
    int end = pkt.indexOf(",", hIdx + 1);
    if (end < 0) end = pkt.length();
    sosHum = pkt.substring(hIdx + 3, end);
  } else sosHum = "n";

  sosReceive = true;
  sosFresh = true;
  sos_heart = millis();

  // Serial.println(F("sos_cashed suscess"));
  // Serial.print(F(" BPM=")); Serial.print(sosBPM);
  // Serial.print(F(" LAT=")); Serial.print(sosLat);
  // Serial.print(F(" LON=")); Serial.print(sosLon);
  // Serial.print(F(" T=")); Serial.print(sosTemp);
  // Serial.print(F(" H=")); Serial.println(sosHum);
}

// telemetry to gnd
void send(){
  pac++;

  String lat = gps.location.isValid() ? String(gps.location.lat(), 6) : "n";
  String lon = gps.location.isValid() ? String(gps.location.lng(), 6) : "n";

  String packet = "cansat";
  packet += "," + String(snapTemp, 1);
  packet += "," + String(snapHum, 1);
  packet += "," + String(snapPres, 1);
  packet += "," + String(snapAlt, 1);
  packet += "," + lat;
  packet += "," + lon;
  packet += ",SEQ:" + String(pac);

  if (sosFresh){
    packet += ",SOS:1";
    packet += ",BPM:" + sosBPM;
    packet += ",SLAT:" + sosLat;
    packet += ",SLON:" + sosLon;
    packet += ",STEMP:" + sosTemp;
    packet += ",SHUM:" + sosHum;
  } 
  else{
    packet += ",SOS:0";
  }

  radio.standby();
  radio.setSyncWord(sync_gnd);
  int state = radio.transmit(packet);

  // Serial.print(F("tx gnd#")); Serial.print(pac);
  // Serial.print(F(" ")); Serial.println(packet);
  // if (state != RADIOLIB_ERR_NONE){
  //   Serial.print(F("tx err ")); Serial.println(state);
  // }
  led_flash();
}

// send ack
void send_ack() {
  if (!sosFresh) return;
  if (millis()-ack < ack_ms) return;

  radio.standby();
  radio.setSyncWord(sync_sos);
  int st = radio.transmit("ack");
  ack = millis();
  Serial.print(F("ack_sos stat=")); Serial.println(st);
}

void waiting_switch(){
  if (digitalRead(pw_pin) == HIGH) return;
  Serial.println(F("switch off"));
  while (digitalRead(pw_pin) == LOW) {delay(100);}
  Serial.println(F("switch on"));
  delay(200);
}

void run_frame() {
  unsigned long frameStart = millis();

  // refresh sensors at frame start
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  bms_snap();
  sos_update();

  receive = false;
  radio.standby();
  radio.setSyncWord(sync_sos);
  radio.startReceive();

  while (millis()-frameStart < rx_ms){
    while (gpsSerial.available()) gps.encode(gpsSerial.read());

    if (receive){
      receive = false;
      
      String incoming;
      int rxState = radio.readData(incoming);
      
      if (rxState == RADIOLIB_ERR_NONE && incoming.startsWith("SOS")){
        Serial.print(F("rx sos")); Serial.println(incoming);
        sos_cashe(incoming);
      } 
      else{
        Serial.print(F("ex err code:")); Serial.println(rxState);
      }
      radio.startReceive();
    }
    delay(2);
  }
  send();

  #define tx_ms 50UL
  if (millis()-frameStart < (frame - tx_ms)){
    send_ack();
  } 
  else{
    Serial.println(F("ack slipped"));
  }
// 1 sec boundry
  while (millis() - frameStart < frame){
    updateLED();
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    delay(1);
  }
}

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(){
  receive = true;
}

void setup(){
  Serial.begin(115200);
  pinMode(pw_pin, INPUT);
  delay(500);
  Serial.println(F("CatSat starting"));

  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
  Wire.begin(SDA, SCL);

  //axp power rail
  axp_w(0x92, (1800 - 500)/100);
  axp_w(0x90, axp_r(0x90) | 0x20);
  axp_w(0x93, (3300-500)/100);
  axp_w(0x90, axp_r(0x90) | 0x40);
  axp_w(0x94, (3300 - 500)/100);
  axp_w(0x90, axp_r(0x90)|0x80);
  delay(500);
  Serial.println(F("axp set"));

  gpsSerial.begin(gps_baud, SERIAL_8N1, gps_rx, gps_tx);
  Serial.println(F("gps locking"));

  if (bme.begin(0x76) || bme.begin(0x77)){
    bmeReady = true;
    bme.setSampling(
      Adafruit_BME280::MODE_NORMAL,
      Adafruit_BME280::SAMPLING_X2,
      Adafruit_BME280::SAMPLING_X16,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::FILTER_X16,
      Adafruit_BME280::STANDBY_MS_500
    );
    base_pres = bme.readPressure()/100.0F;
    snapTemp = bme.readTemperature();
    snapHum  = bme.readHumidity();
    snapPres = bme.readPressure()/100.0F;
    snapAlt  = bme.readAltitude(base_pres);
    bme_update = millis();
    Serial.print(F("bme280 set base: "));
    Serial.print(base_pres); Serial.println(F(" hPa"));
  } 
  else{
    Serial.println(F("bme not found"));
  }

  SPI.begin(SCK, MISO, MOSI, CS);
  int state = radio.begin(
    frq, bandw,
    spread, code_rate,
    sync_sos
  );
  if (state == RADIOLIB_ERR_NONE){
    Serial.println(F("LoRa set"));
    radio.setDio0Action(setFlag, RISING);
  } 
  else{
    Serial.print(F("LoRa fail")); Serial.println(state);
    while (true);
  }

  digitalWrite(led, HIGH);
  Serial.println(F("CanSat set"));
}

void loop() {
  waiting_switch();
  updateLED();
  run_frame();
}
