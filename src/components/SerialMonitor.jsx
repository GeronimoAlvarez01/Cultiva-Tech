import React,{useState,useEffect,useRef} from 'react';
export default function SerialMonitor({lines,onClear,onSend}){
 const [paused,setPaused]=useState(false),[snapshot,setSnapshot]=useState([]),[cmd,setCmd]=useState('');
 const box=useRef(null);
 const text=(paused?snapshot:lines).map(l=>`[${l.time}] ${l.text}`).join('\n');
 // Sigue siempre la última línea (si no, parece congelado cuando pasa de las primeras líneas).
 useEffect(()=>{const el=box.current;if(el&&!paused)el.scrollTop=el.scrollHeight;},[lines,paused]);
 function download(){const u=URL.createObjectURL(new Blob([text],{type:'text/plain'})),a=document.createElement('a');a.href=u;a.download='CultivaTech_serial.txt';a.click();setTimeout(()=>URL.revokeObjectURL(u),1000);}
 return <section className="card"><div className="card-title"><h2>Monitor serial</h2><div className="toolbar"><button onClick={()=>{if(!paused)setSnapshot(lines);setPaused(!paused);}}>{paused?'Continuar vista':'Pausar vista'}</button><button onClick={onClear}>Limpiar</button><button onClick={download}>Descargar registro</button></div></div><pre ref={box} tabIndex="0" aria-live="off">{text||'Sin líneas recibidas. En modo demostración no se inventa salida serial.'}</pre>{onSend&&<form className="toolbar url-form" onSubmit={e=>{e.preventDefault();onSend(cmd);setCmd('');}}><label className="grow">Enviar al serial del ESP32<input value={cmd} onChange={e=>setCmd(e.target.value)} maxLength="200" autoComplete="off" placeholder="AUTO, REARME, RED_OFF, RED_ON, TEST_ALARMA, WEBHOOK …"/></label><button type="submit" disabled={!cmd.trim()}>Enviar</button></form>}<p className="muted">{lines.length} líneas en memoria. Los secretos (token, webhook) se ocultan en el registro.</p></section>;
}
