#include <RadioLib.h>
#include <Wire.h>
#include <SPI.h>

#define cs 18
#define DIO0 26
#define reset 4
#define DIO1 33
#define sck 5
#define MISO 19
#define MOSI 27
#define SDA 21
#define SCL 22
#define led 4
#define axp_adr 0x34

#define frq 905.0
#define band_w 250.0
#define spread 7
#define c_rate 5
#define sync_gnd 0xAB

#define led_flash 300

SX1276 radio = new Module(cs, DIO0, reset, DIO1);
// state
unsigned long ledOnAt = 0;
bool ledOn = false;

// axp
void axp_w(uint8_t reg, uint8_t val){
  Wire.beginTransmission(axp_adr);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}
uint8_t axp_r(uint8_t reg) {
  Wire.beginTransmission(axp_adr);
  Wire.write(reg); Wire.endTransmission(false);
  Wire.requestFrom(axp_adr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}


// led
void flashLED() {digitalWrite(led, HIGH); ledOn = true; ledOnAt = millis();}

void updateLED(){
  if (ledOn && millis() - ledOnAt >= led_flash){
    digitalWrite(led, LOW); ledOn = false;
  }
}

String extract(const String& pkt, const String& key){
  int idx = pkt.indexOf(key);
  if (idx == -1) return "n";
  int start = idx + key.length();
  int end = pkt.indexOf(",", start);
  if (end == -1) end = pkt.length();
  return pkt.substring(start, end);
}


void packSat(const String& packet){
  String fields[7];
  int fieldIdx = 0;
  String token = "";

  for (int i = 0; i <= (int)packet.length() && fieldIdx < 7; i++){
    char c = (i < (int)packet.length()) ? packet[i] : ',';
    if (c == ','){
      fields[fieldIdx++] = token;
      token = "";
    } else token += c;
  }
  
  
  // fields[0]="cansat",[1]=temp,[2]=hum,[3]=pres,[4]=alt,[5]=lat,[6]=lon
  Serial.print(F("cansat,"));
  Serial.print(fields[1]); Serial.print(F(",")); // temp
  Serial.print(fields[2]); Serial.print(F(",")); // humidity
  Serial.print(fields[3]); Serial.print(F(",")); // pressure
  Serial.print(fields[4]); Serial.print(F(",")); // altitude
  Serial.print(fields[5]); Serial.print(F(",")); // lat
  Serial.println(fields[6]); // lon

  String sosFlag = extract(packet, "SOS:");
  if (sosFlag == "1"){
    String bpm = extract(packet, "BPM:");
    String slat = extract(packet, "s_lat:");
    String slon = extract(packet, "s_lon:");
    String stemp = extract(packet, "s_temp:");
    String shum = extract(packet, "s_hum:");

    Serial.print(F("sos_info,"));
    Serial.print(fields[1]); Serial.print(F(",")); // cansat temp
    Serial.print(fields[2]); Serial.print(F(",")); // cansat hum
    Serial.print(fields[3]); Serial.print(F(",")); // cansat pres
    Serial.print(fields[4]); Serial.print(F(","));// cansat alt
    Serial.print(fields[5]); Serial.print(F(",")); // cansat lat
    Serial.print(fields[6]); Serial.print(F(",")); // cansat lon
    Serial.print(bpm); Serial.print(F(",")); // sos BPM
    Serial.print(slat); Serial.print(F(",")); // sos lat
    Serial.print(slon); Serial.print(F(",")); // sos lon
    Serial.print(stemp); Serial.print(F(","));// sos temp
    Serial.println(shum); // sos hum
  }
}

volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag() {receivedFlag = true;}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  Wire.begin(SDA, SCL);

  axp_w(0x93,(3300 - 500)/100);
  axp_w(0x90, axp_r(0x90) | 0x40);
  delay(500);

  SPI.begin(sck, MISO, MOSI,cs);
  int state = radio.begin(
    frq, band_w,
    spread, c_rate,
    sync_gnd
  );

  if (state == RADIOLIB_ERR_NONE){
    digitalWrite(led, HIGH);
    radio.setDio0Action(setFlag, RISING);
    radio.startReceive();
    Serial.println(F("GND Set"));
    Serial.println(F("Porting with Cansat"));
  } 
  else{
    Serial.print(F("GND fail")); Serial.println(state);
    while (true);
  }
}

void loop() {
  updateLED();

  if (!receivedFlag) return;
  receivedFlag = false;

  String incoming;
  int state = radio.readData(incoming);

  if (state == RADIOLIB_ERR_NONE) {
    flashLED();
    if (incoming.startsWith("cansat")){
      packSat(incoming);
    }
  } 
  else{
    Serial.print(F("rx err")); Serial.println(state);
  }
  radio.startReceive();
}
