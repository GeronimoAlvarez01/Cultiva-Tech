// Validación de las tramas CT_DATA que imprime el ESP32 por el serial.
export function validateTelemetry(s,checkClock=false){
  if(s.v!==1||s.source!=='wokwi'||typeof s.boot!=='string'||!/^[a-f0-9]{16}$/i.test(s.boot))throw new Error('Dispositivo no reconocido');
  if(checkClock&&(!Number.isSafeInteger(s.ts)||Math.abs(Date.now()-s.ts)>15000))throw new Error('Lectura caducada o reloj desajustado');
  if(!Number.isSafeInteger(s.seq)||s.seq<1)throw new Error('Secuencia inválida');
  // Un nivel de tanque nulo solo es válido si el ESP32 declara el sensor inválido (sin eco del HC-SR04).
  for(const f of ['soil','tank','light']){
    if(f==='tank'&&s.tank===null&&s.sensorOk===false)continue;
    if(typeof s[f]!=='number'||!Number.isFinite(s[f])||s[f]<0||s[f]>100)throw new Error('Valor fuera de rango');
  }
  for(const f of ['pump','lamp','sensorOk','automatic'])if(typeof s[f]!=='boolean')throw new Error('Estado inválido');
  if(s.temp!==null&&(typeof s.temp!=='number'||!Number.isFinite(s.temp)||s.temp<-40||s.temp>80))throw new Error('Temperatura fuera de rango');
  if(s.humidity!==null&&(typeof s.humidity!=='number'||!Number.isFinite(s.humidity)||s.humidity<0||s.humidity>100))throw new Error('Humedad fuera de rango');
  // Campos añadidos por el firmware nuevo (opcionales para compatibilidad con registros antiguos).
  if(s.alarmMask!==undefined&&(!Number.isInteger(s.alarmMask)||s.alarmMask<0||s.alarmMask>63))throw new Error('Máscara de alarmas inválida');
  if(s.sim!==undefined&&(!Number.isInteger(s.sim)||s.sim<0||s.sim>15))throw new Error('Máscara de simulación inválida');
  if(s.latency!==undefined&&s.latency!==null&&(typeof s.latency!=='number'||!Number.isFinite(s.latency)||s.latency<0))throw new Error('Latencia inválida');
  return s;
}
