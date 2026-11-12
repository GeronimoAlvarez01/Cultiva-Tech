/* CultivaTech 1.0 | Banco educativo de cultivo de cafe
 * Datos exclusivamente simulados. Control local separado de la tarea de red.
 * La clave se introduce por Serial y permanece solo en RAM.
 * PAIR <sala de 8-32 caracteres> <clave de 64 caracteres hexadecimales>
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <mbedtls/md.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#pragma once
#include <stdint.h>
#include <math.h>

// Umbrales didacticos; no son una prescripcion agronomica para cafe.
struct Inputs {
  float soil = 50, tank = 70, light = 70;
  bool sensorOk = false, networkRequired = false, networkUp = false;
};
struct ControlLogic {
  bool automatic = true, requestWater = false, requestLight = false;
  bool pump = false, lamp = false;
  bool timeoutLatch = false, lowTankLatch = false;
  uint32_t waterStarted = 0, manualUntil = 0;
  const char* reason = "STARTUP";
  static constexpr uint32_t MAX_WATER_MS = 60000;
  static constexpr uint32_t MANUAL_LEASE_MS = 30000;
  void setMode(bool autoMode) {
    automatic = autoMode; requestWater = false; requestLight = false;
    pump = lamp = false;
  }
  void manualWater(bool on, uint32_t now) {
    if (!automatic) { requestWater = on; manualUntil = now + MANUAL_LEASE_MS; }
  }
  void acknowledge() { timeoutLatch = false; requestWater = false; }
  void tick(const Inputs& in, uint32_t now) {
    bool valid = in.sensorOk && isfinite(in.soil) && isfinite(in.tank) &&
      isfinite(in.light) && in.soil >= 0 && in.soil <= 100 &&
      in.tank >= 0 && in.tank <= 100 && in.light >= 0 && in.light <= 100;
    if (valid && in.tank < 15) lowTankLatch = true;
    if (valid && in.tank >= 20) lowTankLatch = false;
    if (pump && uint32_t(now - waterStarted) >= MAX_WATER_MS) timeoutLatch = true;
    bool netFault = in.networkRequired && !in.networkUp;
    reason = !valid ? "SENSOR_FAULT" : lowTankLatch ? "LOW_TANK" :
      timeoutLatch ? "MAX_RUNTIME" : netFault ? "NETWORK_LOSS" : "OK";
    if (!valid || lowTankLatch || timeoutLatch || netFault) {
      pump = false; requestWater = false;
    } else {
      bool desired;
      if (automatic) desired = pump ? in.soil < 60 : in.soil < 40;
      else {
        if (int32_t(now - manualUntil) >= 0) requestWater = false;
        desired = requestWater;
      }
      if (desired && !pump) waterStarted = now;
      pump = desired;
    }
    if (!valid) lamp = false;
    else if (automatic) lamp = lamp ? in.light < 40 : in.light < 25;
    else lamp = requestLight;
  }
};

#pragma once
// DigiCert Global Root G2: Mozilla trust store, issuer chain observed at broker.emqx.io.
static const char ROOT_CA[] PROGMEM = R"PEM(-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)PEM";


const int SOIL_PIN=34, LIGHT_PIN=35, DHT_PIN=4, TRIG_PIN=5, ECHO_PIN=18;
const int PUMP_PIN=26, LAMP_PIN=27;

const int RELAY_ON=LOW, RELAY_OFF=HIGH;
const char* MQTT_HOST="broker.emqx.io";
const uint16_t MQTT_PORT=8883;
DHT dht(DHT_PIN,DHT22);
ControlLogic ctl;
Inputs input;
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
SemaphoreHandle_t snapshotMutex;
QueueHandle_t commandQueue;
String latestBody, room, telemetryTopic, commandTopic, bootId;
uint8_t sessionKey[32];
volatile bool paired=false, networkUp=false, forcedOffline=false;
uint32_t seq=0, lastSample=0;
uint64_t lastCommandTime=0;
float tempC=0, humidity=0, rawSoil=50, distanceCm=70;
float soilHistory[3]={50,50,50};
int soilIndex=0, soilCount=0;
bool measured=false;
struct Command { char action[16]; bool on; bool autoMode; };

uint64_t unixMs(){ struct timeval tv; gettimeofday(&tv,nullptr); return uint64_t(tv.tv_sec)*1000+tv.tv_usec/1000; }
String macFor(const String& topic,const String& body){
  String message=topic+"\n"+body;
  unsigned char digest[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),sessionKey,32,
    (const unsigned char*)message.c_str(),message.length(),digest);
  char hex[65]; for(int i=0;i<32;i++) sprintf(hex+i*2,"%02x",digest[i]);
  hex[64]=0; return String(hex);
}
bool sameMac(const String& a,const String& b){
  if(a.length()!=64 || b.length()!=64) return false;
  uint8_t diff=0; for(int i=0;i<64;i++) diff |= a[i]^b[i]; return diff==0;
}
void received(char* topic,byte* data,unsigned int len){
  if(len>1500 || String(topic)!=commandTopic) return;
  JsonDocument outer;
  if(deserializeJson(outer,data,len)) return;
  if(!outer["body"].is<const char*>() || !outer["mac"].is<const char*>()) return;
  String body=outer["body"].as<String>(), mac=outer["mac"].as<String>();
  if(!sameMac(mac,macFor(commandTopic,body))){ Serial.println("SECURITY: comando rechazado (firma)"); return; }
  JsonDocument command;
  if(deserializeJson(command,body)) return;
  uint64_t ts=command["ts"].as<uint64_t>(), now=unixMs();
  if(command["boot"].as<String>()!=bootId || ts<=lastCommandTime || ts>now+5000 || now>ts+15000){
    Serial.println("SECURITY: comando rechazado (sesion/caducidad/repeticion)"); return;
  }
  String action=command["action"]|"";
  if(action!="mode" && action!="water" && action!="lamp" && action!="ack") return;
  Command c={}; action.toCharArray(c.action,sizeof(c.action));
  c.on=command["on"]|false; c.autoMode=command["automatic"]|true;
  if(xQueueSend(commandQueue,&c,0)==pdTRUE) lastCommandTime=ts;
}
void networkTask(void*){
  uint32_t lastAttempt=0, lastPublish=0;
  bool wifiStarted=false;
  secureClient.setCACert(ROOT_CA);
  secureClient.setHandshakeTimeout(30);
  mqtt.setServer(MQTT_HOST,MQTT_PORT); mqtt.setCallback(received);
  mqtt.setBufferSize(2048); mqtt.setKeepAlive(15); mqtt.setSocketTimeout(3);
  for(;;){
    if(!paired){ vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    if(forcedOffline){
      networkUp=false;
      if(mqtt.connected()) mqtt.disconnect();
      if(wifiStarted){ WiFi.disconnect(); wifiStarted=false; }
      vTaskDelay(pdMS_TO_TICKS(100)); continue;
    }
    if(!wifiStarted){
      WiFi.mode(WIFI_STA); WiFi.begin("Wokwi-GUEST","",6);
      sntp_set_sync_interval(15000);
      configTime(0,0,"pool.ntp.org","time.google.com");
      wifiStarted=true; lastAttempt=0;
      Serial.println("NET: conectando Wi-Fi y sincronizando reloj...");
    }
    if(WiFi.status()!=WL_CONNECTED || time(nullptr)<1700000000){
      networkUp=false; vTaskDelay(pdMS_TO_TICKS(100)); continue;
    }
    if(!mqtt.connected()){
      networkUp=false;
      if(lastAttempt==0 || uint32_t(millis()-lastAttempt)>10000){
        lastAttempt=millis();
        String clientId="ct-esp-"+bootId;
        if(mqtt.connect(clientId.c_str())){
          mqtt.subscribe(commandTopic.c_str(),1); networkUp=true;
          Serial.println("NET: MQTT/TLS conectado. Telemetria autenticada activa.");
        } else { Serial.printf("NET: MQTT no disponible (%d). Reintento seguro.\n",mqtt.state()); char tlsError[160]; secureClient.lastError(tlsError,sizeof(tlsError)); Serial.printf("NET TLS: %s\n",tlsError); }
      }
      vTaskDelay(pdMS_TO_TICKS(100)); continue;
    }
    mqtt.loop(); networkUp=mqtt.connected();
    if(uint32_t(millis()-lastPublish)>=2000){
      String body;
      if(xSemaphoreTake(snapshotMutex,pdMS_TO_TICKS(50))==pdTRUE){body=latestBody;xSemaphoreGive(snapshotMutex);}
      if(body.length()){
        JsonDocument envelope; envelope["body"]=body; envelope["mac"]=macFor(telemetryTopic,body);
        String out; serializeJson(envelope,out);
        if(!mqtt.publish(telemetryTopic.c_str(),out.c_str(),false)) Serial.println("NET: muestra no enviada");
      }
      lastPublish=millis();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
void pairLine(String line){
  line.trim();
  if(line=="HELP") {Serial.println("PAIR <sala> <clave hex de 64 caracteres> | ACK");return;}
  if(line=="ACK"){ctl.acknowledge();return;}
  if(!line.startsWith("PAIR ")) return;
  if(paired){ Serial.println("PAIR: ya vinculado; reinicia para cambiar de sala."); return; }
  int split=line.indexOf(' ',5);
  if(split<0) return;
  String newRoom=line.substring(5,split), key=line.substring(split+1);
  if(newRoom.length()<8 || newRoom.length()>32 || key.length()!=64){Serial.println("PAIR: formato invalido");return;}
  for(unsigned int i=0;i<newRoom.length();i++) if(!isalnum(newRoom[i]) && newRoom[i]!='-') return;
  for(int i=0;i<64;i++) if(!isxdigit(key[i])) return;
  for(int i=0;i<32;i++) sessionKey[i]=strtoul(key.substring(i*2,i*2+2).c_str(),nullptr,16);
  room=newRoom;
  telemetryTopic="cultivatech/v1/"+room+"/telemetry";
  commandTopic="cultivatech/v1/"+room+"/command";
  paired=true;
  Serial.println("PAIR: sesion recibida. Clave en RAM; no se imprime ni se guarda.");
}
void sampleSensors(){
  tempC=dht.readTemperature(); humidity=dht.readHumidity();
  if(!isfinite(tempC)) Serial.println("DHT: lectura invalida");
  rawSoil=analogRead(SOIL_PIN)*100.0f/4095.0f;
  soilHistory[soilIndex]=rawSoil; soilIndex=(soilIndex+1)%3; if(soilCount<3)soilCount++;
  float sum=0;for(int i=0;i<soilCount;i++)sum+=soilHistory[i]; input.soil=sum/soilCount;
  input.light=100.0f-analogRead(LIGHT_PIN)*100.0f/4095.0f;
  digitalWrite(TRIG_PIN,LOW); delayMicroseconds(2); digitalWrite(TRIG_PIN,HIGH);
  delayMicroseconds(10); digitalWrite(TRIG_PIN,LOW);
  unsigned long duration=pulseIn(ECHO_PIN,HIGH,25000);
  distanceCm=duration*0.0343f/2.0f;
  input.tank=constrain((35.0f-distanceCm)*100.0f/30.0f,0.0f,100.0f);
  measured=duration>0 && distanceCm>=2 && distanceCm<=400 &&
    isfinite(tempC)&&tempC>=-40&&tempC<=80&&isfinite(humidity)&&humidity>=0&&humidity<=100;
}
void publishSnapshot(){
  JsonDocument doc;
  doc["v"]=1; doc["source"]="wokwi"; doc["device"]="esp32-cultivatech";
  doc["boot"]=bootId; doc["seq"]=++seq; doc["ts"]=unixMs(); doc["uptime"]=millis();
  doc["soil"]=roundf(input.soil*10)/10; doc["soilRaw"]=roundf(rawSoil*10)/10;
  if(isfinite(tempC))doc["temp"]=roundf(tempC*10)/10;else doc["temp"]=nullptr;
  if(isfinite(humidity))doc["humidity"]=roundf(humidity*10)/10;else doc["humidity"]=nullptr;
  doc["light"]=roundf(input.light*10)/10; doc["tank"]=roundf(input.tank*10)/10;
  doc["distance"]=roundf(distanceCm*10)/10;
  doc["sensorOk"]=input.sensorOk; doc["automatic"]=ctl.automatic;
  doc["pump"]=ctl.pump; doc["lamp"]=ctl.lamp;
  doc["reason"]=ctl.reason; doc["wifi"]=WiFi.status()==WL_CONNECTED;
  doc["mqtt"]=networkUp; doc["rssi"]=WiFi.status()==WL_CONNECTED?WiFi.RSSI():0;
  doc["waterElapsed"]=ctl.pump?uint32_t(millis()-ctl.waterStarted)/1000:0;
  String body;serializeJson(doc,body);
  if(xSemaphoreTake(snapshotMutex,pdMS_TO_TICKS(20))==pdTRUE){latestBody=body;xSemaphoreGive(snapshotMutex);}
  Serial.println("CT_DATA "+body);
  Serial.printf("SAMPLE #%lu suelo=%.1f%% tanque=%.1f%% temp=%.1fC bomba=%s motivo=%s\n",
    (unsigned long)seq,input.soil,input.tank,tempC,ctl.pump?"ON":"OFF",ctl.reason);
}
void setup(){
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  for(int pin:{PUMP_PIN,LAMP_PIN}){digitalWrite(pin,RELAY_OFF);pinMode(pin,OUTPUT);}
  pinMode(TRIG_PIN,OUTPUT);pinMode(ECHO_PIN,INPUT);

  analogReadResolution(12);dht.begin();
  char boot[17];sprintf(boot,"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());bootId=boot;
  snapshotMutex=xSemaphoreCreateMutex(); commandQueue=xQueueCreate(8,sizeof(Command));
  xTaskCreatePinnedToCore(networkTask,"mqtt",12288,nullptr,1,nullptr,0);
  Serial.println("CULTIVATECH 1.1 | Prototipo educativo ESP32");
  Serial.println("CT_READY");
  Serial.println("Control local activo. Pega PAIR desde Conectar Wokwi del visor 3D.");
  Serial.println("Suelo: potenciometro; HC-SR04: lleno 5 cm, vacio 35 cm. Solo bomba y luz. ACK por Serial.");
}
void loop(){
  static String line;
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'){pairLine(line);line="";}else if(c!='\r' && line.length()<120)line+=c;
  }
  forcedOffline=false;
  if(forcedOffline)networkUp=false;

  Command c;
  while(xQueueReceive(commandQueue,&c,0)==pdTRUE){
    if(!strcmp(c.action,"mode"))ctl.setMode(c.autoMode);
    else if(!strcmp(c.action,"water"))ctl.manualWater(c.on,millis());
    else if(!strcmp(c.action,"lamp") && !ctl.automatic)ctl.requestLight=c.on;
    else if(!strcmp(c.action,"ack"))ctl.acknowledge();
  }
  bool sample=uint32_t(millis()-lastSample)>=2000;
  if(sample){sampleSensors();lastSample=millis();}
  input.sensorOk=measured;
  input.networkUp=networkUp && !forcedOffline;
  // Control automatico local independiente del broker; manual requiere enlace.
  input.networkRequired=!ctl.automatic;
  ctl.tick(input,millis());
  digitalWrite(PUMP_PIN,ctl.pump?RELAY_ON:RELAY_OFF);

  digitalWrite(LAMP_PIN,ctl.lamp?RELAY_ON:RELAY_OFF);
  if(sample)publishSnapshot();
  delay(10);
}
