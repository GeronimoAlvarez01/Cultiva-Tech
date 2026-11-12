import React from 'react';
// Panel en vivo propio del visor: muestra los mismos datos del ESP32 que llegan por el serial (cada ~2 s).
// No es el dashboard de Ubidots (ese se actualiza cada 30 s); sirve para ver todo moviéndose en la presentación.
const SERIES=[['temp','Temperatura','°C','#c0432d',[0,50]],['soil','Humedad de suelo','%','#2f7d4f',[0,100]],['tank','Nivel del tanque','%','#1b86b8',[0,100]],['light','Luz relativa','%','#b98a0f',[0,100]]];
function Chart({rows,k,label,unit,color,range}){
 const W=300,H=96,P=6,pts=rows.map((r,i)=>[i,r[k]]).filter(p=>p[1]!=null);
 const last=rows.length?rows[rows.length-1][k]:null;
 const x=i=>P+(rows.length<2?0:i*(W-2*P)/(rows.length-1)),y=v=>H-P-(Math.max(range[0],Math.min(range[1],v))-range[0])*(H-2*P)/(range[1]-range[0]);
 const d=pts.map((p,j)=>(j?'L':'M')+x(p[0]).toFixed(1)+' '+y(p[1]).toFixed(1)).join(' ');
 return <div className="live-chart"><div className="live-head"><span>{label}</span><strong style={{color}}>{last==null?'—':Number(last).toFixed(k==='temp'?1:0)} <small>{unit}</small></strong></div>
  <svg viewBox={'0 0 '+W+' '+H} role="img" aria-label={label+': '+(last==null?'sin dato':last+' '+unit)}><rect x="0" y="0" width={W} height={H} rx="8" fill="#f3f6ef"/>{[.25,.5,.75].map(f=><line key={f} x1={P} x2={W-P} y1={H*f} y2={H*f} stroke="#dbe4d6" strokeDasharray="3 4"/>)}{d&&<path d={d} fill="none" stroke={color} strokeWidth="2.4" strokeLinejoin="round" strokeLinecap="round"/>}</svg></div>;
}
export default function LiveDashboard({history,data,stale}){
 const live=!!data&&!stale;
 return <section className="card live-dash" aria-labelledby="live-title">
  <div className="card-title"><h2 id="live-title">Panel en vivo del ESP32 (visor)</h2><span className={'pill '+(live?'ok':'bad')}>{live?'recibiendo datos':'sin datos recientes'}</span></div>
  <p className="muted">Mismos datos que ve el 3D, actualizados cada ~2 s directamente del serial de Wokwi. Útil para mostrar el sistema «moviéndose» mientras Ubidots refresca cada 30 s.</p>
  <div className="live-grid">{SERIES.map(([k,l,u,c,r])=><Chart key={k} rows={history} k={k} label={l} unit={u} color={c} range={r}/>)}</div>
  <div className="status-row"><b>💧 Bomba: {!live?'sin confirmar':data.pump?'ENCENDIDA':'apagada'}</b><b>💡 Iluminación: {!live?'sin confirmar':data.lamp?'ENCENDIDA':'apagada'}</b><b>{live&&data.alarms?'⚠ '+data.alarms+' alerta(s): '+data.reason:'✓ Sin alertas'}</b></div>
 </section>;
}
