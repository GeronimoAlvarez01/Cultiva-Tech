import React,{useRef,useState,useEffect} from 'react';
import {useCultiva} from './hooks/useCultiva.js';
import FarmView from './components/FarmView.jsx';
import Readings from './components/Readings.jsx';
import SerialMonitor from './components/SerialMonitor.jsx';
import UbidotsPanel from './components/UbidotsPanel.jsx';
import logoCultivaTech from './assets/Logo_CultivaTech.png';
import DocsPanel from './components/DocsPanel.jsx';
import {STALE_MS} from './config.js';
import {SIM_GROUPS,SIM_NORMAL,nextSimState,simAction} from './simulador.js';
const TABS=[['simulacion','Simulación'],['dashboard','Dashboard'],['docs','Documentación']];
const tabFromHash=()=>{const h=(globalThis.location?.hash||'').slice(1);return TABS.some(t=>t[0]===h)?h:'simulacion';};
const linkText=v=>v===true?'conectado':v===false?'sin conexión':'sin dato';
const linkClass=v=>v===true?'ok':v===false?'bad':'';
export default function App(){
 const [tab,setTab]=useState(tabFromHash),[mode,setMode]=useState('wokwi'),[message,setMessage]=useState(''),[input,setInput]=useState(''),[token,setToken]=useState(''),[now,setNow]=useState(Date.now()),[reloadKey,setReloadKey]=useState(0),[wantStart,setWantStart]=useState(false),[simState,setSimState]=useState({}),[history,setHistory]=useState([]);
 const iframe=useRef(null),slot=useRef(null);const [offscreen,setOffscreen]=useState(false),[mini,setMini]=useState(false),[side,setSide]=useState('right'),[slotH,setSlotH]=useState(560);
 const sim=useCultiva(mode,iframe,reloadKey);
 useEffect(()=>{const t=setInterval(()=>setNow(Date.now()),1000);return()=>clearInterval(t);},[]);
 useEffect(()=>{const f=()=>setTab(tabFromHash());window.addEventListener('hashchange',f);return()=>window.removeEventListener('hashchange',f);},[]);
 // «Iniciar simulación» pulsado antes de que el circuito termine de cargar: arranca solo cuando esté listo.
 useEffect(()=>{if(wantStart&&sim.phase==='ready'){setWantStart(false);sim.control.current.start?.().catch(e=>setMessage(e.message));}},[wantStart,sim.phase,sim.control]);
 useEffect(()=>{setWantStart(false);setSimState({});setHistory([]);},[mode,reloadKey]);
 useEffect(()=>{const d=sim.data;if(!d)return;setHistory(h=>[...h,{temp:d.temp,soil:d.soil,tank:d.tank,light:d.light}].slice(-120));},[sim.data]);
 // Si el ESP32 reinicia o confirma que no hay simulación activa, los botones vuelven a su estado inicial.
 useEffect(()=>{if(mode==='wokwi'&&sim.data?.sim===0)setSimState(s=>Object.keys(s).some(k=>s[k])?{}:s);},[sim.data,mode]);
 // Wokwi frena su simulación cuando su recuadro no se ve en pantalla (el navegador limita los iframes ocultos).
 // Por eso, si el recuadro sale de la vista (scroll o cambio de pestaña) pasa a una ventanita flotante que sigue visible.
 useEffect(()=>{const el=slot.current;if(!el||typeof IntersectionObserver==='undefined')return;const io=new IntersectionObserver(([e])=>{if(e.isIntersecting&&e.intersectionRatio>=.35)setSlotH(Math.round(e.boundingClientRect.height)||560);setOffscreen(!(e.isIntersecting&&e.intersectionRatio>=.35));},{threshold:[0,.35,.7,1]});io.observe(el);return()=>io.disconnect();},[mode,reloadKey]);
 const stale=!sim.data||(mode!=='demo'&&(now-sim.received>STALE_MS||sim.phase==='paused'));
 async function act(name,...args){try{await sim.control.current[name]?.(...args);setMessage('');}catch(e){setMessage(e.message);}}
 function go(id){setTab(id);try{history.replaceState(null,'','#'+id);}catch{/* sin historial */}}
 function start(){setMessage('');if(mode==='wokwi'){if(sim.phase==='ready')act('start');else setWantStart(true);}}
 function sendToken(e){e.preventDefault();const t=token.trim();if(!t)return;if(/\s/.test(t)){setMessage('El token de Ubidots no lleva espacios.');return;}act('write','UBIDOTS_TOKEN '+t).then(()=>setToken(''));}
 function sendLine(line){const t=line.trim();if(t)act('write',t);}
 const running=['running','paused'].includes(sim.phase);
 const floating=mode==='wokwi'&&running&&offscreen;
 // Botones del simulador: en Wokwi mandan SIM_* por serial; en demostración cambian los datos sintéticos.
 async function applyDemo(values){for(const [k,v] of Object.entries(values))await sim.control.current.setDemo?.(k,v);}
 async function simulate(id){
  const action=simAction(simState,id);if(!action)return;
  try{
   if(mode==='demo')await applyDemo(action.demo);else await sim.control.current.write(action.cmd);
   setSimState(s=>nextSimState(s,id));setMessage('');
  }catch(e){setMessage(e.message);}
 }
 async function normalize(){
  try{if(mode==='demo')await applyDemo(SIM_NORMAL.demo);else await sim.control.current.write(SIM_NORMAL.cmd);setSimState({});setMessage('');}catch(e){setMessage(e.message);}
 }
 // Si llegan muestras sin el campo «sim», el firmware.bin es anterior a esta versión y no entiende los botones.
 const oldFirmware=mode==='wokwi'&&running&&!!sim.data&&sim.data.sim===undefined;
 const canSim=(mode==='demo'||running)&&!oldFirmware;
 const tabKey=e=>{const i=TABS.findIndex(t=>t[0]===tab);let n=i;if(e.key==='ArrowRight')n=(i+1)%TABS.length;else if(e.key==='ArrowLeft')n=(i+TABS.length-1)%TABS.length;else return;e.preventDefault();go(TABS[n][0]);document.getElementById('tab-'+TABS[n][0])?.focus();};
 const d=sim.data,live=!stale&&d;
 const alerts=live?(d.alarms??0):0;
 return <><header className="hero"><div className="hero-inner">
  <div className="brand"><img src={logoCultivaTech} alt="Logo CultivaTech" className="logo"/><div><h1>Cultiva <b>Tech</b></h1><p>Gemelo digital de riego inteligente · ESP32 · Wokwi · Ubidots</p></div></div>
  <div className="chips" aria-label="Estado del sistema">
   <span className={'chip '+((running||mode==='demo')&&!stale?'ok':'')}>● {mode==='demo'?'Modo demostración':'ESP32 '+(running?(stale?'sin datos':'en marcha'):'detenido')}</span>
   <span className={'chip '+linkClass(live?d.hivemq:undefined)}>HiveMQ {live?linkText(d.hivemq):'—'}</span>
   <span className={'chip '+linkClass(live?d.ubidots:undefined)}>Ubidots {live?linkText(d.ubidots):'—'}</span>
   <span className={'chip '+(live&&d.pump?'water':'')}>💧 Riego {live?(d.pump?'activo':'detenido'):'—'}</span>
   <span className={'chip '+(live&&d.lamp?'lamp':'')}>💡 Luz {live?(d.lamp?'encendida':'apagada'):'—'}</span>
   <span className={'chip '+(alerts?'bad':live?'ok':'')}>{alerts?'⚠ '+alerts+' alerta'+(alerts>1?'s':''):'✓ Sin alertas'}</span>
  </div></div>
  <div role="tablist" aria-label="Secciones" className="tabs" onKeyDown={tabKey}>{TABS.map(([id,label])=><button key={id} role="tab" id={'tab-'+id} aria-selected={tab===id} aria-controls={'panel-'+id} tabIndex={tab===id?0:-1} className={tab===id?'tab active':'tab'} onClick={()=>go(id)}>{label}</button>)}</div>
 </header>
 <main>
 <div role="tabpanel" id="panel-simulacion" aria-labelledby="tab-simulacion" className={tab==='simulacion'?'':'keep-alive'} aria-hidden={tab!=='simulacion'}>
  <section className="card control-bar">
   <div className="toolbar">
    {mode==='wokwi'?<>
     <button className="primary" disabled={running||wantStart||!['ready','loading'].includes(sim.phase)} onClick={start}>{running?'● Simulación en marcha':wantStart?'Esperando al circuito…':'▶ Iniciar simulación'}</button>
     <button disabled={!running} onClick={()=>act(sim.phase==='paused'?'resume':'pause')}>{sim.phase==='paused'?'Reanudar':'Pausar'}</button>
     <button disabled={!running} onClick={()=>act('restart')}>Reiniciar ESP32</button>
     <button onClick={()=>{setReloadKey(k=>k+1);setMessage('');}}>Recargar circuito</button>
    </>:<span>Fuente: {mode==='demo'?'demostración local (sin Wokwi)':'registro serial'}.</span>}
   </div>
   <p role="status">{sim.detail||'Preparando…'}</p>
   {oldFirmware&&<p className="notice" role="alert"><b>El firmware.bin que está corriendo es una versión anterior</b> y no entiende los botones del simulador. Recompila <code>wokwi/final/sketch.ino</code> en Wokwi, descarga el firmware (F1 → «Download Compiled Firmware»), guárdalo como <code>wokwi/final/firmware.bin</code>, ejecuta <code>npm run sync:wokwi</code> y pulsa «Recargar circuito».</p>}
   {message&&<p className="notice" role="alert">{message}</p>}
  </section>
  <div className={'workspace '+(mode==='wokwi'?'split':'')}>
   {mode==='wokwi'&&<div ref={slot} className="wokwi-slot" style={floating?{minHeight:slotH}:undefined}><section className={'card wokwi-card'+(floating?' floating'+(mini?' mini':'')+(side==='left'?' left':''):'')}><div className="card-title"><h2>Simulación de Wokwi</h2>{floating?<span className="float-tools"><span className="pill ok">sigue corriendo</span><button aria-label="Mover la ventana al otro lado" title="Mover al otro lado" onClick={()=>setSide(x=>x==='right'?'left':'right')}>⇄</button><button aria-label={mini?'Agrandar':'Minimizar'} title={mini?'Agrandar':'Minimizar (sigue corriendo)'} onClick={()=>setMini(m=>!m)}>{mini?'▢':'—'}</button></span>:null}</div><iframe ref={iframe} title="Circuito Wokwi de Cultiva Tech" allow="fullscreen"/><p className="muted">Ejecuta el <code>firmware.bin</code> compilado de <code>wokwi/final/sketch.ino</code>. Si aparece un error de firmware, mira el README.</p></section></div>}
   <FarmView origin={mode} data={sim.data} stale={stale} packet={sim.packet}/>
  </div>
  <SerialMonitor lines={sim.lines} onClear={sim.clearLines} onSend={mode==='wokwi'&&running?sendLine:null}/>
  <section className="card sim-panel" aria-labelledby="sim-title">
   <div className="card-title"><h2 id="sim-title">Simulador de condiciones</h2><button className="reset" disabled={!canSim} onClick={normalize}>↺ Normalizar todo</button></div>
   <p className="muted">Cada botón cambia lo que «mide» un sensor y el ESP32 reacciona como en la vida real: riega, enciende la luz, bloquea la bomba o levanta una alerta. {mode==='wokwi'&&!running&&'Inicia la simulación para activarlos. '}Con «Normalizar todo» los sensores vuelven a obedecer al circuito de Wokwi.</p>
   <div className="sim-buttons">{SIM_GROUPS.map(g=>{const a=simAction(simState,g.id);return <button key={g.id} disabled={!canSim} className={'sim-btn'+(simState[g.id]?' active':'')+(g.danger?' danger':'')} aria-pressed={!!simState[g.id]} onClick={()=>simulate(g.id)}><span aria-hidden="true">{g.icon}</span>{a.label}</button>;})}</div>
   {live&&d.sim>0&&<p className="muted">Simulación activa en el ESP32: los sensores marcados ya no siguen al circuito de Wokwi.</p>}
  </section>
  <Readings data={sim.data} stale={stale}/>
  <section className="card"><h2>Enlaces de red del ESP32</h2>
   <div className="status-row"><span className={'pill '+linkClass(live?d.hivemq:undefined)}>HiveMQ (MQTT Explorer): {live?linkText(d.hivemq):'sin dato'}</span><span className={'pill '+linkClass(live?d.ubidots:undefined)}>Ubidots (dashboard): {live?linkText(d.ubidots):'sin dato'}</span></div>
   <p className="muted">Si ambos están sin conexión el ESP32 sigue regando de forma automática con sus umbrales; el modo manual exige red. Ubidots se actualiza cada 30 s y al instante cuando cambia la bomba, la luz o una alarma; este visor se actualiza cada 2 s.</p>
   {mode==='wokwi'&&<><form className="toolbar url-form" onSubmit={sendToken}><label className="grow">Token de Ubidots (va por el serial virtual, queda solo en la RAM del ESP32 y no se muestra)<input type="password" value={token} onChange={e=>setToken(e.target.value)} autoComplete="off" spellCheck="false" placeholder="BBUS-…"/></label><button type="submit" disabled={!running||!token.trim()}>Enviar token</button></form>
   <div className="toolbar" aria-label="Comandos rápidos por serial">{['AUTO','REARME','RED_OFF','RED_ON','TEST_ALARMA'].map(c=><button key={c} disabled={!running} onClick={()=>sendLine(c)}>{c}</button>)}</div></>}
  </section>
  {mode==='import'&&<section className="card"><h2>Reproducir lecturas registradas</h2><p>Pega líneas completas que empiecen por CT_DATA. Es una reproducción, no una conexión en vivo.</p><textarea aria-label="Registro serial" value={input} onChange={e=>setInput(e.target.value)} rows="6"/><button onClick={()=>act('ingest',input)}>Cargar registro</button></section>}
  <section className="card"><h2>Eventos</h2><ul>{sim.events.length?sim.events.map((e,i)=><li key={i}>{e.time} · {e.text}</li>):<li>Sin eventos.</li>}</ul></section>
  <details className="card"><summary>Otras fuentes de datos (para editar el diseño sin Wokwi)</summary><label className="mode">Origen de las lecturas <select value={mode} onChange={e=>setMode(e.target.value)}><option value="wokwi">Wokwi integrado — serial real del simulador</option><option value="demo">Demostración local — datos sintéticos (los botones también funcionan)</option><option value="import">Reproducir un registro serial</option></select></label></details>
 </div>
 <div role="tabpanel" id="panel-dashboard" aria-labelledby="tab-dashboard" hidden={tab!=='dashboard'}><UbidotsPanel active={tab==='dashboard'} ubidotsLink={live?d.ubidots:undefined} history={history} data={sim.data} stale={stale}/></div>
 <div role="tabpanel" id="panel-docs" aria-labelledby="tab-docs" hidden={tab!=='docs'}><DocsPanel active={tab==='docs'}/></div>
 <footer>Edita los componentes en src/components, la escena en src/lib/FarmScene.js y el firmware en wokwi/final. Solo simulación; el ECHO de 5 V del HC-SR04 requiere adaptación para un ESP32 físico.</footer></main></>;
}
