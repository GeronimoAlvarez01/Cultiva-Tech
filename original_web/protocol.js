import {hmac} from './vendor/noble/hmac.js';
import {sha256} from './vendor/noble/sha256.js';
export const BROKER='wss://broker.emqx.io:8084/mqtt';
export const PREFIX='cultivatech/v1/';
export function fromHex(hex){if(!/^[a-f0-9]{64}$/i.test(hex))throw new Error('Clave inválida');return new Uint8Array(hex.match(/../g).map(v=>parseInt(v,16)));}
export function hex(bytes){return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');}
export async function importKey(secret){return fromHex(secret);}
export async function signEnvelope(key,topic,data){const body=JSON.stringify(data);const mac=hex(hmac(sha256,key,new TextEncoder().encode(topic+'\n'+body)));return JSON.stringify({body,mac});}
export async function verifyEnvelope(key,topic,raw){
  if(raw.length>4096)throw new Error('Paquete demasiado grande');
  const env=JSON.parse(raw);
  if(typeof env.body!=='string'||typeof env.mac!=='string')throw new Error('Formato de mensaje inválido');
  const received=fromHex(env.mac), expected=hmac(sha256,key,new TextEncoder().encode(topic+'\n'+env.body));
  let mismatch=0;for(let i=0;i<32;i++)mismatch|=received[i]^expected[i];const valid=mismatch===0;
  if(!valid)throw new Error('Firma no válida');
  const s=JSON.parse(env.body);
  return validateTelemetry(s,true);
}
export function validateTelemetry(s,checkClock=false){
  if(s.v!==1||s.source!=='wokwi'||typeof s.boot!=='string'||!/^[a-f0-9]{16}$/i.test(s.boot))throw new Error('Dispositivo no reconocido');
  if(checkClock&&(!Number.isSafeInteger(s.ts)||Math.abs(Date.now()-s.ts)>15000))throw new Error('Lectura caducada o reloj desajustado');
  if(!Number.isSafeInteger(s.seq)||s.seq<1)throw new Error('Secuencia inválida');
  for(const f of ['soil','tank','light'])if(typeof s[f]!=='number'||!Number.isFinite(s[f])||s[f]<0||s[f]>100)throw new Error('Valor fuera de rango');
  for(const f of ['pump','lamp','sensorOk','automatic'])if(typeof s[f]!=='boolean')throw new Error('Estado inválido');
  if(s.temp!==null&&(typeof s.temp!=='number'||!Number.isFinite(s.temp)||s.temp<-40||s.temp>80))throw new Error('Temperatura fuera de rango');
  if(s.humidity!==null&&(typeof s.humidity!=='number'||!Number.isFinite(s.humidity)||s.humidity<0||s.humidity>100))throw new Error('Humedad fuera de rango');
  return s;
}
