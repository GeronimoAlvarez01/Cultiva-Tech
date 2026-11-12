#include <DHT.h>
// SOLO SIMULACION: ECHO 5 V directo no es seguro en hardware fisico.
constexpr int DHT_PIN=4,SOIL=34,LDR=35,TRIG=5,ECHO=18,PUMP=26,LAMP=27;
DHT dht(DHT_PIN,DHT22);
bool pump=false,lamp=false,lowTank=false;
unsigned long lastRead=0;
void setup(){
 Serial.begin(115200);dht.begin();analogReadResolution(12);
 digitalWrite(PUMP,HIGH);digitalWrite(LAMP,HIGH);
 pinMode(PUMP,OUTPUT);pinMode(LAMP,OUTPUT);
 pinMode(TRIG,OUTPUT);pinMode(ECHO,INPUT);
}
void loop(){
 if(millis()-lastRead<2000)return;lastRead=millis();
 float soil=analogRead(SOIL)*100.0f/4095;
 float light=100-analogRead(LDR)*100.0f/4095;
 float temp=dht.readTemperature(),humidity=dht.readHumidity();
 digitalWrite(TRIG,LOW);delayMicroseconds(2);digitalWrite(TRIG,HIGH);
 delayMicroseconds(10);digitalWrite(TRIG,LOW);
 unsigned long us=pulseIn(ECHO,HIGH,25000);
 float distance=us*0.0343f/2;
 float tank=constrain((35-distance)*100/30,0.0f,100.0f);
 bool valid=us>0 && distance>=2 && distance<=400 && isfinite(temp)&&isfinite(humidity);
 if(tank<15)lowTank=true;else if(tank>=20)lowTank=false;
 if(!valid||lowTank)pump=false;else if(soil<40)pump=true;else if(soil>=60)pump=false;
 if(!valid)lamp=false;else if(light<25)lamp=true;else if(light>=40)lamp=false;
 digitalWrite(PUMP,pump?LOW:HIGH);digitalWrite(LAMP,lamp?LOW:HIGH);
 Serial.printf("suelo=%.1f%% luz=%.1f%% temp=%.1fC HR=%.1f%% distancia=%.1fcm tanque=%.1f%% bomba=%s iluminacion=%s valido=%d\n",soil,light,temp,humidity,distance,tank,pump?"ON":"OFF",lamp?"ON":"OFF",valid);
}
