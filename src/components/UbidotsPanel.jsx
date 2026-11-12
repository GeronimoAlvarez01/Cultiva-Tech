import React,{useState} from 'react';
import {UBIDOTS_DASHBOARD_URL} from '../config.js';
import LiveDashboard from './LiveDashboard.jsx';
import {isUbidotsUrl,readSavedUbidotsUrl,saveUbidotsUrl} from '../lib/ubidots.js';
// Pestaña 2: dashboard público de Ubidots (gauge de temperatura, gráfica histórica e interruptores).
export default function UbidotsPanel({active,ubidotsLink,history,data,stale}){
 const initial=UBIDOTS_DASHBOARD_URL&&isUbidotsUrl(UBIDOTS_DASHBOARD_URL)?UBIDOTS_DASHBOARD_URL:readSavedUbidotsUrl();
 const [url,setUrl]=useState(initial),[draft,setDraft]=useState(initial),[error,setError]=useState(''),[blocked,setBlocked]=useState(false),[embed,setEmbed]=useState(false);
 function save(e){
  e.preventDefault();const value=draft.trim();
  if(!value){setUrl('');saveUbidotsUrl('');setError('');return;}
  if(!isUbidotsUrl(value)){setError('El enlace debe empezar por https:// y ser de ubidots.com (por ejemplo https://stem.ubidots.com/app/dashboards/public/dashboard/...).');return;}
  setError('');setBlocked(false);setUrl(value);saveUbidotsUrl(value);
 }
 return <><section className="card" aria-labelledby="ubi-title">
  <div className="card-title"><h2 id="ubi-title">Dashboard de Ubidots en tiempo real</h2><span className={'pill '+(ubidotsLink===true?'ok':ubidotsLink===false?'bad':'')}>ESP32 → Ubidots: {ubidotsLink===true?'conectado':ubidotsLink===false?'sin conexión':'sin dato (inicia la simulación)'}</span></div>
  <p>Aquí se muestra el dashboard que construyeron en Ubidots (gauge de temperatura, gráfica histórica e interruptores de bomba y luz). El ESP32 de Wokwi publica en <code>/v1.6/devices/cultivatech</code> y escucha las órdenes en <code>cultivatech-ctrl</code>.</p>
  <form className="toolbar url-form" onSubmit={save}>
   <label className="grow">Enlace público del dashboard (Ubidots → Dashboards → Compartir → Público)<input type="url" value={draft} onChange={e=>setDraft(e.target.value)} placeholder="https://stem.ubidots.com/app/dashboards/public/dashboard/…" autoComplete="off" spellCheck="false"/></label>
   <button type="submit">Guardar enlace</button>
   
  </form>
  {error&&<p className="notice" role="alert">{error}</p>}
  <p className="muted">Seguridad: quien tenga un enlace público con interruptores puede operar la bomba y la luz. Compártelo solo con el docente o usa un dashboard público solo de lectura para la demostración.</p>
  {url?<div className="ubi-actions"><p><b>Ubidots no permite mostrar su dashboard dentro de otras páginas</b> (por eso el navegador dice «rechazó la conexión»). Ábrelo en su propia ventana y ponla junto a esta, o usa el panel en vivo de abajo.</p>
   <div className="toolbar"><button className="primary" onClick={()=>{const w=window.open(url,'ubidots-cultivatech','popup=yes,width=1100,height=800');if(!w)window.open(url,'_blank','noopener,noreferrer');}}>Abrir dashboard de Ubidots en ventana nueva</button><button onClick={()=>setEmbed(e=>!e)}>{embed?'Ocultar intento de incrustar':'Intentar mostrarlo aquí'}</button></div>
   {embed&&active&&<iframe title="Dashboard de Ubidots de Cultiva Tech" src={url} className="tall" sandbox="allow-scripts allow-same-origin allow-forms allow-popups" referrerPolicy="no-referrer" onError={()=>setBlocked(true)}/>}{embed&&blocked&&<p className="muted">El navegador reportó un error al cargarlo.</p>}</div>
  :<div className="empty"><p><b>Todavía no hay enlace.</b> Pega arriba el enlace público de su dashboard. Se guarda solo en este navegador.</p></div>}
 </section>
 <LiveDashboard history={history} data={data} stale={stale}/></>;

}
