import React,{useState} from 'react';
import {DOCS} from '../config.js';
// Pestaña 3: documentación. Botones SEMANA 10 / 12 / 13 / 14 y, debajo, los PDF de la semana elegida.
export default function DocsPanel({active}){
 const weeks=[...new Set(DOCS.map(d=>d.week))];
 const [week,setWeek]=useState(weeks[0]);const list=DOCS.filter(d=>d.week===week);
 const [id,setId]=useState(list[0].id);const doc=list.find(d=>d.id===id)||list[0];
 const href='./docs/'+doc.file;
 const pick=w=>{setWeek(w);setId(DOCS.find(d=>d.week===w).id);};
 return <section className="card" aria-labelledby="docs-title">
  <h2 id="docs-title">Documentación del proyecto</h2>
  <div className="week-buttons" role="group" aria-label="Semanas">{weeks.map(w=><button key={w} className={w===week?'week active':'week'} aria-pressed={w===week} onClick={()=>pick(w)}>{w.toUpperCase()}</button>)}</div>
  {list.length>1&&<div className="toolbar" role="group" aria-label="Documentos de la semana">{list.map(d=><button key={d.id} className={d.id===doc.id?'doc active':'doc'} aria-pressed={d.id===doc.id} onClick={()=>setId(d.id)}>{d.title}<small> · {d.pages} páginas</small></button>)}</div>}
  <div className="toolbar"><a className="btn" href={href} target="_blank" rel="noopener noreferrer">Abrir en pestaña nueva</a><a className="btn" href={href} download>Descargar PDF</a></div>
  {active&&<iframe title={doc.week+': '+doc.title} src={href} className="tall"/>}
  <p className="muted">Si tu navegador no muestra el PDF aquí, usa «Abrir» o «Descargar».</p>
 </section>;
}
