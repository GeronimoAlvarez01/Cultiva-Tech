import {FarmScene} from './scene.js';
import {sensorInfo} from './sensors.js';
import {WokwiBridge,WOKWI_PROJECT} from './wokwi-bridge.js';
import {SerialParser} from './serial.js';
import {BROKER,PREFIX,hex,importKey,signEnvelope,verifyEnvelope} from './protocol.js';
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
let farm,bridge,runtime='loading',state=null,source='serial',receivedAt=0,mqttAt=0,mqttCount=0,client=null;
let room='',secret='',key,serialLines=[],serialPaused=false,serialCount=0,events=[],records=[],seen=new Map(),processing=Promise.resolve(),pairedBoot=false,lastReason='',lastPump=null,toastTimer,external=false;
const reasons={OK:['Control local activo','El ESP32 decide el estado de cada relé a partir de sus sensores.'],SENSOR_FAULT:['Fallo de sensor · riego bloqueado','Revisa el DHT22 y el HC-SR04.'],LOW_TANK:['Tanque bajo · riego bloqueado','La bomba vuelve a estar disponible cuando el tanque llega al 20 %.'],MAX_RUNTIME:['Límite de 60 s alcanzado','Rearma el temporizador desde Control manual → Reconocer fallo.'],NETWORK_LOSS:['MQTT no disponible · riego bloqueado','En modo manual, la bomba se apaga mientras se recupera la conexión.'],STARTUP:['Inicio seguro','Validando sensores antes de actuar.']};
const time=ts=>new Date(ts).toLocaleTimeString('es-CO');
function toast(text){$('#toast').textContent=text;$('#toast').classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('#toast').classList.remove('show'),4500);}
function addEvent(text,type='INFO'){events.unshift({ts:Date.now(),text,type});events=events.slice(0,80);const box=$('#events');box.replaceChildren();for(const e of events.slice(0,25)){const row=document.createElement('div');row.className='event '+(e.type==='ALERTA'?'warning':'');const t=document.createElement('time');t.textContent=time(e.ts);const p=document.createElement('span');p.textContent=e.text;row.append(t,p);box.append(row);}}
function metric(id,value,unit,decimals=0){$(id).replaceChildren(document.createTextNode(value==null?'—':Number(value).toFixed(decimals)));const small=document.createElement('small');small.textContent=unit;$(id).append(small);}
function stale(){return !state||(!external&&runtime!=='running')||Date.now()-receivedAt>12000;}
function paint(){
  const old=stale(),s=state;$('.telemetry-panel').classList.toggle('stale-values',old);
  metric('#soil',s?.soil,'%');metric('#temp',s?.temp,'°C',1);metric('#humidity',s?.humidity,'%');metric('#tank',s?.tank,'%');metric('#light',s?.light,'%');
  $('#soil-bar').style.width=(s?.soil||0)+'%';$('#tank-bar').style.width=(s?.tank||0)+'%';
  $('#mode-tag').textContent=!s?'Esperando Wokwi':runtime==='paused'?'Pausado':old?'Última lectura':'Wokwi · en vivo';$('#mode-tag').className='mode-tag '+(old?'stale':'live');
  $('#signal-dot').className='status-dot '+(old?'stale':'');
  $('#reading-source').textContent=!s?'Las tarjetas se activan al recibir la primera muestra.':source==='mqtt'?'MQTT / TLS · firma HMAC verificada':'Puerto serial del ESP32 · valores de Wokwi';
  $('#last-update').textContent=s?'Muestra #'+s.seq+' · hace '+Math.floor((Date.now()-receivedAt)/1000)+' s':'—';
  $('#scene-source').textContent=!s?'Sin lecturas todavía':old?'Último estado · sin actualización reciente':source==='mqtt'?'Sincronizado por MQTT':'Sincronizado por puerto serial';
  $('#control-mode').textContent=!s?'SIN DATOS':s.automatic?'AUTOMÁTICO':'MANUAL';
  for(const [field,id,on,off] of [['pump','#pump-state','Encendida','Apagada'],['lamp','#lamp-state','Encendida','Apagada']]){$(id).textContent=old?'Sin confirmar':s[field]?on:off;$(id).className=!old&&s[field]?'on':'';}
  const r=!s?['Esperando al ESP32','Pulsa «Iniciar Wokwi» para activar las lecturas.']:runtime==='paused'?['Simulación pausada','Pulsa «Reanudar» para continuar.']:old?['Esperando una nueva lectura','Los valores visibles son la última muestra. La velocidad depende del simulador.']:(reasons[s.reason]||reasons.OK);
  $('#safety-note b').textContent=r[0];$('#safety-note small').textContent=r[1];$('#safety-note').classList.toggle('alert',!!s&&(old||s.reason!=='OK'));$('#safety-note>span').textContent=old?'i':s.reason==='OK'?'✓':'!';
  $('#wifi-status').textContent='Wi-Fi: '+(!s||old?'sin confirmar':s.wifi?'conectado':'sin conexión');$('#wifi-status').classList.toggle('ok',!!s&&!old&&s.wifi);
  const freshMqtt=Date.now()-mqttAt<12000;$('#mqtt-status').textContent='MQTT: '+(freshMqtt?'recibiendo':client?.connected?'visor conectado; ESP32 pendiente':'esperando broker');$('#mqtt-status').classList.toggle('ok',freshMqtt);
  $('#packet-count').textContent=mqttCount+' mensajes MQTT';$('#integrity-label').textContent=freshMqtt?'HMAC verificado':'Sin mensaje MQTT reciente';$('#integrity-label').classList.toggle('ok',freshMqtt);
  $('#topic-label').textContent=PREFIX+room+'/telemetry';$('#live-status').textContent=freshMqtt?'ESP32 vinculado por MQTT':'Esperando telemetría MQTT del ESP32';
  for(const id of ['#water-on','#water-off','#lamp-on','#lamp-off'])$(id).disabled=old||!s||s.automatic||!freshMqtt;
  $('#set-auto').classList.toggle('selected',!!s?.automatic);$('#set-manual').classList.toggle('selected',s&&!s.automatic);
  $('#control-help').textContent='Órdenes por MQTT autenticado. El visor espera la confirmación del ESP32 y mantiene todas las protecciones.';
  if(farm)farm.setState(s||{soil:0,tank:0,pump:false,lamp:false},true,old);
}
function accept(s,via){
  if(state?.boot===s.boot&&s.seq<state.seq){if(via==='mqtt'){farm?.packet('mqtt');paint();}return;}
  const same=state?.boot===s.boot&&state?.seq===s.seq;
  if(!same){state=s;source=via;receivedAt=Date.now();records.push({...s,receivedAt,via});records=records.slice(-900);if(lastReason!==s.reason){addEvent((reasons[s.reason]||reasons.OK)[0],s.reason==='OK'?'CONTROL':'ALERTA');lastReason=s.reason;}if(lastPump!==s.pump){addEvent(s.pump?'Riego activado: bomba encendida.':'Riego detenido: bomba apagada.','ACTUADOR');lastPump=s.pump;}}
  if(via==='serial')farm?.packet('serial');else{farm?.packet('mqtt');$('.flow-card').classList.remove('pulse');requestAnimationFrame(()=>$('.flow-card').classList.add('pulse'));}
  paint();
}
function serialLine(line){
  serialCount++;serialLines.push({ts:Date.now(),line});serialLines=serialLines.slice(-1000);
  $('#serial-count').textContent=serialCount+' líneas recibidas';
  if(line==='CT_READY'){
    pairedBoot=false;runtime='running';$('#runtime-label').textContent='ESP32 iniciado';$('#runtime-detail').textContent='Lecturas y vinculación automáticas';
    if(!client)connect();
    $('#pause-sim').disabled=false;$('#restart-sim').disabled=false;$('#start-sim').disabled=true;
    // Session key remains in RAM. Only the verified Wokwi MessagePort receives it.
    if(!pairedBoot){pairedBoot=true;bridge.write('PAIR '+room+' '+secret).then(()=>addEvent('ESP32 vinculado automáticamente; esperando MQTT.','MQTT')).catch(e=>{pairedBoot=false;addEvent(e.message,'ALERTA');});}
  }
  if(line.startsWith('NET:')||line.startsWith('NET TLS:'))addEvent(line,line.includes('conectado.')?'MQTT':'INFO');
  renderSerial();
}
function friendly(line){
  if(line.startsWith('SAMPLE #'))return null;
  if(!line.startsWith('CT_DATA '))return line;
  try{const s=JSON.parse(line.slice(8));return '#'+s.seq+'  Suelo '+s.soil+' % · Temp. '+(s.temp??'—')+' °C · Humedad '+(s.humidity??'—')+' % · Tanque '+s.tank+' % · Luz '+s.light+' %\n             Bomba '+(s.pump?'ON':'OFF')+' · Luz '+(s.lamp?'ON':'OFF')+' · '+s.reason;}catch{return line;}
}
function renderSerial(){if(serialPaused)return;const raw=$('#serial-raw').checked,lines=serialLines.map(e=>{const text=raw?e.line:friendly(e.line);return text===null?null:'['+time(e.ts)+'] '+text;}).filter(Boolean);$('#serial-output').textContent=lines.join('\n')||'Esperando salida serial de Wokwi…';$('#serial-output').scrollTop=$('#serial-output').scrollHeight;}
const parser=new SerialParser(serialLine,s=>{runtime='running';$('#runtime-label').textContent='Wokwi en ejecución';$('#runtime-detail').textContent='Muestra #'+s.seq+' recibida';$('#serial-status').textContent='Recibiendo del ESP32 · muestra #'+s.seq;accept(s,'serial');},e=>addEvent('Serial: '+e,'ALERTA'));
async function pairing(fresh=false){
  if(!secret||fresh){secret=hex(crypto.getRandomValues(new Uint8Array(32)));room='ct-'+hex(crypto.getRandomValues(new Uint8Array(6)));}
  key=await importKey(secret);$('#room-label').textContent=room;$('#pair-command').value='PAIR '+room+' '+secret;
}
function disconnect(){client?.end(true);client=null;mqttAt=0;seen.clear();}
async function connect(){
  if(!key)await pairing();if(client)return;$('#connection-status').textContent='Conectando al broker…';
  const active=client=window.mqtt.connect(BROKER,{clientId:'ct-view-'+hex(crypto.getRandomValues(new Uint8Array(6))),clean:true,keepalive:20,reconnectPeriod:5000,connectTimeout:12000,protocolVersion:4});
  active.on('connect',()=>{if(active!==client)return;active.subscribe(PREFIX+room+'/telemetry',{qos:0},err=>{if(err){addEvent('No se pudo abrir la suscripción MQTT.','ALERTA');return;}$('#connection-status').textContent='Visor conectado al broker';addEvent('Visor suscrito a MQTT sobre WSS. Esperando al ESP32.','MQTT');paint();});});
  active.on('message',(topic,payload,packet)=>{if(active!==client)return;processing=processing.then(async()=>{try{if(packet.retain||topic!==PREFIX+room+'/telemetry')throw Error('Canal o retención no permitidos');const s=await verifyEnvelope(key,topic,payload.toString());if(s.seq<=(seen.get(s.boot)||0))return;seen.set(s.boot,s.seq);if(seen.size>8)seen.delete(seen.keys().next().value);mqttAt=Date.now();mqttCount++;if(external){runtime='running';$('#runtime-label').textContent='Wokwi externo conectado';$('#runtime-detail').textContent='Recibiendo MQTT autenticado';appendExternalSerial(s);}accept(s,'mqtt');$('#connection-status').textContent='MQTT del ESP32 verificado';}catch(e){addEvent('MQTT rechazado: '+e.message,'ALERTA');}});});
  active.on('error',()=>{$('#connection-status').textContent='Broker no disponible; reintentando';paint();});
  active.on('offline',()=>{$('#connection-status').textContent='Broker desconectado; reintentando';paint();});
}
function appendExternalSerial(s){serialLines.push({ts:Date.now(),line:'MQTT '+JSON.stringify(s)});serialLines=serialLines.slice(-1000);serialCount++;$('#serial-count').textContent=serialCount+' mensajes MQTT (sin acceso UART externo)';$('#serial-status').textContent='Canal externo: telemetría MQTT';renderSerial();}
function bridgeStatus(phase,text){
  runtime=phase;$('#runtime-detail').textContent=text;
  const labels={loading:'Cargando Wokwi…',ready:'Circuito listo',starting:'Iniciando ESP32…',running:'Wokwi en ejecución',paused:'Simulación pausada',error:'No se pudo iniciar Wokwi'};$('#runtime-label').textContent=labels[phase]||phase;
  $('#start-sim').disabled=!['ready','error'].includes(phase);$('#start-sim').textContent=phase==='error'?'↻ Reintentar Wokwi':'▶ Iniciar Wokwi';
  $('#pause-sim').disabled=!['running','paused'].includes(phase);$('#pause-sim').textContent=phase==='paused'?'▶ Reanudar':'Ⅱ Pausar';$('#restart-sim').disabled=!['running','paused'].includes(phase);
  $('#serial-status').textContent=phase==='paused'?'Simulación pausada':phase==='error'?'Simulador no disponible':phase==='running'?'Esperando salida del ESP32':'Esperando simulación';
  $('#start-help').textContent=phase==='error'?text:phase==='running'?'Cambia un sensor en el circuito: el campo 3D y la consola se actualizan automáticamente.':'El circuito y el firmware se cargan automáticamente. No necesitas copiar archivos.';
  paint();
}
function initBridge(){bridge?.destroy();bridge=new WokwiBridge($('#wokwi-embed'),{status:bridgeStatus,serial:bytes=>parser.push(bytes)});}
async function start(){
  if(runtime==='error'){initBridge();return;}
  external=false;state=null;receivedAt=0;parser.reset();pairedBoot=false;serialLines=[];serialCount=0;records=[];lastReason='';lastPump=null;mqttCount=0;
  await pairing();connect();try{await bridge.start();addEvent('Simulación Wokwi iniciada con el circuito completo.');}catch(e){bridgeStatus('error',e.message);}
}
async function command(action,value){
  if(!client?.connected||!state||stale()||Date.now()-mqttAt>12000){toast('Espera un mensaje MQTT verificado del ESP32 para enviar órdenes.');return;}
  const data={v:1,boot:state.boot,ts:Date.now(),action};if(action==='mode')data.automatic=value;else data.on=value;
  const topic=PREFIX+room+'/command',envelope=await signEnvelope(key,topic,data);client.publish(topic,envelope,{qos:1,retain:false},err=>{if(err){toast('No se pudo enviar la orden.');return;}$('#command-status').textContent='Orden enviada; esperando confirmación del ESP32.';addEvent('Orden '+action+' enviada por MQTT.');});
}
function download(name,data,type='application/octet-stream'){const url=URL.createObjectURL(new Blob([data],{type}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
async function downloadWokwi(){try{const names=['sketch.ino','diagram.json','ControlLogic.h','ca.h','libraries.txt'];const files={};await Promise.all(names.map(async n=>{const r=await fetch('wokwi/'+n);if(!r.ok)throw Error(n);files[n]=new Uint8Array(await r.arrayBuffer());}));download('CultivaTech_Wokwi.zip',window.fflate.zipSync(files));}catch{toast('No se pudo descargar el proyecto.');}}
function sensor(id){const info=sensorInfo[id]||sensorInfo.sensors;$('#sensor-title').textContent=info[0];$('#sensor-description').textContent=info[1];$('#sensor-detail').textContent=info[2];$('#sensor-dialog').showModal();}
function setView(view){farm?.setView(view);for(const b of $$('[data-view]'))b.classList.toggle('selected',b.dataset.view===view);$('#scene-caption').textContent=view==='network'?'Pulsos azules: mensajes MQTT verificados · pulsos verdes: sensor → ESP32':'Arrastra para girar · Usa la rueda para acercar';}
$('#start-sim').onclick=start;$('#pause-sim').onclick=async()=>{try{if(runtime==='paused')await bridge.resume();else await bridge.pause();}catch(e){toast(e.message);}};
$('#restart-sim').onclick=async()=>{parser.reset();pairedBoot=false;state=null;receivedAt=0;seen.clear();try{await bridge.restart();addEvent('ESP32 reiniciado; vinculación automática renovada.');}catch(e){toast(e.message);}};
$('#help-button').onclick=()=>{$('#quick-guide').hidden=!$('#quick-guide').hidden;};
$('#serial-pause').onclick=()=>{serialPaused=!serialPaused;$('#serial-pause').textContent=serialPaused?'Continuar vista':'Pausar vista';if(!serialPaused)renderSerial();};$('#serial-raw').onchange=renderSerial;$('#serial-clear').onclick=()=>{serialLines=[];serialPaused=false;$('#serial-pause').textContent='Pausar vista';renderSerial();};$('#serial-download').onclick=()=>download('CultivaTech_Serial.txt',serialLines.map(e=>'['+new Date(e.ts).toISOString()+'] '+e.line).join('\n'),'text/plain;charset=utf-8');
for(const b of $$('[data-view]'))b.onclick=()=>setView(b.dataset.view);for(const b of $$('[data-focus]'))b.onclick=()=>{setView('network');farm?.focus(b.dataset.focus);$('#laboratorio').scrollIntoView({behavior:'smooth'});};
$('#reset-camera').onclick=()=>farm?.reset();$('#toggle-labels').onclick=()=>farm?.toggleLabels();$('#fullscreen').onclick=()=>{if(document.fullscreenElement)document.exitFullscreen?.();else $('.scene-card').requestFullscreen?.();};
for(const id of ['#nav-docs','#privacy-info'])$(id).onclick=()=>$('#docs-dialog').showModal();$('#open-controls').onclick=()=>$('#control-dialog').showModal();$('#set-auto').onclick=()=>command('mode',true);$('#set-manual').onclick=()=>command('mode',false);$('#water-on').onclick=()=>command('water',true);$('#water-off').onclick=()=>command('water',false);$('#lamp-on').onclick=()=>command('lamp',true);$('#lamp-off').onclick=()=>command('lamp',false);$('#ack-fault').onclick=()=>command('ack',true);
$('#clear-log').onclick=()=>{events=[];$('#events').replaceChildren();};$('#export-data').onclick=()=>{if(!records.length){toast('Todavía no hay muestras.');return;}const fields=['via','source','ts','receivedAt','seq','soil','temp','humidity','tank','light','pump','lamp','sensorOk','automatic','reason'];download('CultivaTech_Telemetria.csv','\ufeff'+fields.join(';')+'\n'+records.map(r=>fields.map(f=>r[f]??'').join(';')).join('\n'),'text/csv;charset=utf-8');};
$('#open-connect').onclick=()=>$('#connect-dialog').showModal();$('#mqtt-connect').onclick=()=>{if(bridge?.started){toast('Reinicia la página antes de utilizar un proyecto externo.');return;}external=true;source='mqtt';connect();};$('#new-session').onclick=async()=>{disconnect();await pairing(true);if(bridge?.started){parser.reset();await bridge.restart();}else toast('Nueva sala creada. Vincula el Wokwi externo con el comando.');};$('#copy-pair').onclick=async()=>{try{await navigator.clipboard.writeText($('#pair-command').value);toast('Comando copiado.');}catch{$('#pair-command').type='text';$('#pair-command').select();toast('Copia el comando seleccionado.');}};$('#download-wokwi').onclick=downloadWokwi;
$('#show-wiring').onclick=()=>$('#wiring-dialog').showModal();
try{farm=new FarmScene($('#scene'),sensor);}catch(e){$('#scene-loading').textContent='No se pudo iniciar la vista 3D. Las lecturas y el circuito siguen disponibles.';console.error(e);}
await pairing();initBridge();paint();addEvent('Laboratorio preparado. Inicia Wokwi para recibir lecturas reales del simulador.');setInterval(paint,1000);window.addEventListener('pagehide',()=>{disconnect();bridge?.destroy();});
