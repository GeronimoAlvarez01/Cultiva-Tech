import {DEMO_LIMITS as L} from '../config.js';
// Datos sintéticos para editar la escena sin Wokwi ni internet. Nunca pasan por el parser de telemetría real.
// Reproduce la forma de CT_DATA del firmware: alarmMask (bit0 temperatura, 1 suelo, 2 tanque, 3 sensor, 4 riego, 5 latencia),
// temp/tank nulos cuando el sensor falla y las mismas razones de control.
export class DemoModel{
 constructor(){this.inputs={soil:46,tank:69,light:72,temp:24,humidity:65,dhtFail:false,sonicFail:false};this.pump=false;this.lamp=false;this.low=false;this.started=0;this.timeout=false;this.seq=0;this.tempAlarm=false;}
 tick(now=Date.now()){
 const i=this.inputs,sensorOk=!i.dhtFail&&!i.sonicFail;
 if(!i.sonicFail){if(i.tank<L.tankStop)this.low=true;else if(i.tank>=L.tankResume)this.low=false;}
 if(!i.dhtFail){if(i.temp>30)this.tempAlarm=true;if(i.temp<28)this.tempAlarm=false;}
 if(this.pump&&now-this.started>=L.maxWaterMs)this.timeout=true;
 const reason=!sensorOk?'FALLO_SENSOR':this.low?'TANQUE_CRITICO':this.timeout?'TIEMPO_MAXIMO':'OK';
 const next=reason==='OK'&&(this.pump?i.soil<L.soilOff:i.soil<L.soilOn);
 if(next&&!this.pump)this.started=now;this.pump=next;
 this.lamp=sensorOk&&(this.lamp?i.light<L.lightOff:i.light<L.lightOn);
 const mask=(this.tempAlarm&&!i.dhtFail?1:0)|(sensorOk&&i.soil<20?2:0)|(sensorOk&&this.low?4:0)|(!sensorOk?8:0)|(this.timeout?16:0);
 let alarms=0;for(let b=0;b<6;b++)if(mask&(1<<b))alarms++;
 return {v:1,source:'demo',boot:'demo',seq:++this.seq,ts:now,soil:i.soil,light:i.light,
  tank:i.sonicFail?null:i.tank,distance:i.sonicFail?null:35-i.tank*.3,temp:i.dhtFail?null:i.temp,humidity:i.dhtFail?null:i.humidity,
  sensorOk,pump:this.pump,lamp:this.lamp,automatic:true,reason,alarms,alarmMask:mask,sim:0,latency:null,hivemq:false,ubidots:false};
 }
}
