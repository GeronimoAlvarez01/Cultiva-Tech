import {useEffect,useRef,useState} from 'react';
import {WokwiBridge} from '../lib/WokwiBridge.js';
import {SerialParser} from '../lib/serial.js';
import {DemoModel} from '../lib/DemoModel.js';
import {redactSerialInput} from '../lib/serial.js';

// Fuentes de datos: 'wokwi' (serial real del simulador), 'demo' (datos sintéticos) e 'import' (registro pegado).
// El visor solo LEE el serial del ESP32 simulado. Las órdenes llegan por Ubidots o por MQTT Explorer (HiveMQ);
// desde aquí solo se pueden escribir líneas por el monitor serial (token, webhook, pruebas de red).
export function useCultiva(mode,iframeRef,reloadKey=0){
 const [data,setData]=useState(null),[phase,setPhase]=useState('idle'),[detail,setDetail]=useState(''),[lines,setLines]=useState([]),[events,setEvents]=useState([]),[packet,setPacket]=useState({id:0,kind:'serial'}),[received,setReceived]=useState(0);
 const control=useRef({});
 useEffect(()=>{
 let active=true,bridge,timer,watchdog,lastBytes=Date.now(),last=null,currentPhase='idle';
 // Las líneas del serial se acumulan y se pintan 4 veces por segundo como máximo (el arranque del ESP32 manda cientos de golpe).
 let pending=[],flushTimer=null;
 const flush=()=>{flushTimer=null;if(!active||!pending.length)return;const batch=pending;pending=[];setLines(xs=>xs.concat(batch).slice(-300));};
 const log=text=>{pending.push({time:new Date().toLocaleTimeString('es-CO'),text});if(!flushTimer)flushTimer=setTimeout(flush,250);};
 const event=text=>{if(active)setEvents(xs=>[{time:new Date().toLocaleTimeString('es-CO'),text},...xs].slice(0,80));};
 const status=(p,t)=>{currentPhase=p;if(active){setPhase(p);setDetail(t);}};
 const accept=(s,kind)=>{
  if(!active)return;
  if(last?.boot===s.boot&&s.seq<last.seq)return;
  if(last?.boot!==s.boot||s.seq!==last.seq){last=s;setData(s);setReceived(Date.now());}
  setPacket(p=>({id:p.id+1,kind}));
 };
 const parser=new SerialParser(line=>log(line),s=>{status('running','Muestra '+s.seq+(mode==='import'?' reproducida del registro':' recibida del ESP32'));accept(s,'serial');},e=>event('Serial: '+e));
 setData(null);setLines([]);setEvents([]);setReceived(0);
 control.current={
  start:async()=>{parser.reset();last=null;await bridge?.start();},
  pause:()=>bridge?.pause(),resume:()=>bridge?.resume(),
  restart:()=>{parser.reset();last=null;return bridge?.restart();},
  ingest:text=>parser.push(new TextEncoder().encode(text+'\n')),
  // Escribe una línea en el serial del ESP32 simulado. Los secretos no se muestran en el monitor.
  write:async line=>{
   if(!bridge||!['running','paused'].includes(currentPhase))throw Error('Inicia la simulación antes de enviar líneas al ESP32.');
   await bridge.write(line);log('→ '+redactSerialInput(line));
  }
 };
 if(mode==='demo'){
  const model=new DemoModel();const tick=()=>accept(model.tick(),'demo');
  control.current.setDemo=(name,value)=>{model.inputs[name]=value;tick();};
  control.current.ack=()=>{model.timeout=false;tick();};tick();timer=setInterval(tick,1000);status('demo','Datos sintéticos para editar el diseño');
 }else if(mode==='wokwi'){
  status('loading','Cargando Wokwi experimental');
  bridge=new WokwiBridge(iframeRef.current,{status,serial:bytes=>{lastBytes=Date.now();if(active)parser.push(bytes);}});
  // Wokwi pausa la simulación si el iframe deja de verse; si no la pausó el usuario, se reanuda sola.
  watchdog=setInterval(async()=>{
   if(currentPhase!=='running'||Date.now()-lastBytes<6000)return;
   try{const st=await bridge.client.simStatus();if(st&&st.running===false&&currentPhase==='running'){await bridge.resume();log('[visor] La simulación estaba en pausa y se reanudó automáticamente.');}}catch{/* sin respuesta: se reintenta */}
  },4000);
 }else status('import','Pega líneas CT_DATA del monitor serial para reproducirlas');
 return()=>{active=false;clearTimeout(flushTimer);clearInterval(timer);clearInterval(watchdog);bridge?.destroy();control.current={};};
 },[mode,iframeRef,reloadKey]);
 return {data,phase,detail,lines,events,packet,received,control,clearLines:()=>setLines([])};
}
