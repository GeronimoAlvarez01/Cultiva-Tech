// Configuración pública. Nunca pongas contraseñas ni tokens privados en variables VITE_.
export const WOKWI_EMBED_URL=import.meta.env?.VITE_WOKWI_EMBED_URL || 'https://wokwi.com/experimental/embed?client_id=wokwi_client_omf4ejkz6n6twj7d3x2eqmyp';
// Enlace PÚBLICO del dashboard de Ubidots (Dashboards → compartir). Es opcional: también puede pegarse en la pestaña.
export const UBIDOTS_DASHBOARD_URL=import.meta.env?.VITE_UBIDOTS_DASHBOARD_URL || '';
export const STALE_MS=12000;
// Solo el modo demostración usa estos valores. Para Wokwi edita wokwi/final/sketch.ino y recompila.
export const DEMO_LIMITS={soilOn:40,soilOff:60,lightOn:25,lightOff:40,tankStop:15,tankResume:20,maxWaterMs:60000};
// Documentos alojados en public/docs. Son los PDF de entrega de cada semana.
export const DOCS=[
 {id:'s10',week:'Semana 10',title:'Documento técnico: integración y ética (prototipo a producto)',file:'semana-10-integracion-etica.pdf',pages:11},
 {id:'s10-esquema',week:'Semana 10',title:'Diagrama electrónico (3 hojas)',file:'semana-10-diagrama-electronico.pdf',pages:3},
 {id:'s12',week:'Semana 12',title:'Matriz comparativa de protocolos',file:'semana-12-protocolos.pdf',pages:5},
 {id:'s13',week:'Semana 13',title:'MQTT: montaje en Wokwi, MQTT Explorer e informe',file:'semana-13-mqtt.pdf',pages:26},
 {id:'s14',week:'Semana 14',title:'Dashboard en Ubidots, alertas y pruebas de latencia',file:'semana-14-dashboards.pdf',pages:7}
];
