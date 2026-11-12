// Botones del simulador de condiciones. Cada grupo alterna entre dos acciones.
// cmd = línea que se envía por el serial del ESP32 (el firmware responde en menos de 2 s).
// demo = cambio equivalente en el modo demostración (sin Wokwi).
export const SIM_GROUPS=[
 {id:'tank',icon:'🛢️',on:{label:'Vaciar tanque',cmd:'SIM_TANQUE_VACIO',demo:{tank:0,sonicFail:false}},off:{label:'Llenar tanque',cmd:'SIM_TANQUE_LLENO',demo:{tank:100,sonicFail:false}},clears:['sonic']},
 {id:'temp',icon:'🌡️',on:{label:'Aumentar temperatura',cmd:'SIM_TEMP_ALTA',demo:{temp:35,dhtFail:false}},off:{label:'Temperatura normal',cmd:'SIM_TEMP_NORMAL',demo:{temp:24,dhtFail:false}},clears:['dht']},
 {id:'light',icon:'💡',on:{label:'Bajar iluminación',cmd:'SIM_LUZ_BAJA',demo:{light:10}},off:{label:'Subir iluminación',cmd:'SIM_LUZ_ALTA',demo:{light:85}}},
 {id:'soil',icon:'🌱',on:{label:'Suelo seco',cmd:'SIM_SUELO_SECO',demo:{soil:15}},off:{label:'Suelo húmedo',cmd:'SIM_SUELO_HUMEDO',demo:{soil:75}}},
 {id:'dht',icon:'⚠️',on:{label:'Falla del DHT22',cmd:'SIM_FALLA_DHT',demo:{dhtFail:true}},off:{label:'Reparar DHT22',cmd:'SIM_TEMP_NORMAL',demo:{dhtFail:false,temp:24}},clears:['temp'],danger:true},
 {id:'sonic',icon:'⚠️',on:{label:'Falla del HC-SR04',cmd:'SIM_FALLA_ULTRASONICO',demo:{sonicFail:true}},off:{label:'Reparar HC-SR04',cmd:'SIM_TANQUE_LLENO',demo:{sonicFail:false,tank:100}},clears:['tank'],danger:true}
];
export const SIM_NORMAL={cmd:'SIM_NORMAL',demo:{soil:46,tank:69,light:72,temp:24,humidity:65,dhtFail:false,sonicFail:false}};
// Estado de botones tras pulsar un grupo: alterna el grupo y libera los que quedan contradichos.
export function nextSimState(state,id){
 const g=SIM_GROUPS.find(x=>x.id===id);if(!g)return state;
 const turnOn=!state[id];const next={...state,[id]:turnOn};
 for(const other of g.clears||[])next[other]=false;
 return next;
}
export function simAction(state,id){const g=SIM_GROUPS.find(x=>x.id===id);return g?(state[id]?g.off:g.on):null;}
