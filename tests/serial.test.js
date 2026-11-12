import assert from 'node:assert/strict';
import {SerialParser} from '../src/lib/serial.js';
const lines=[],values=[],errors=[];
const p=new SerialParser(x=>lines.push(x),s=>values.push(s),e=>errors.push(e));
const frame={v:1,source:'wokwi',boot:'1234567890abcdef',seq:1,soil:45,tank:68.4,light:75.6,temp:24,humidity:65,pump:false,lamp:false,sensorOk:true,automatic:true};
const text='CT_DATA '+JSON.stringify(frame)+'\r\n';
const bytes=new TextEncoder().encode(text);
for(let i=0;i<bytes.length;i+=7)p.push(bytes.slice(i,i+7));
assert.equal(values.length,1);assert.equal(values[0].tank,68.4);
p.push(bytes);assert.equal(values.length,1);assert.match(errors.pop(),/repetida/);
p.push(new TextEncoder().encode('CT_DATA '+JSON.stringify({...frame,seq:2,soil:130})+'\n'));
assert.equal(values.length,1);assert.match(errors.pop(),/rango/);
p.push(new TextEncoder().encode('x'.repeat(4100)+'\nCT_READY\n'));
assert.equal(lines.at(-1),'CT_READY');assert.match(errors.pop(),/larga/);
p.push(new TextEncoder().encode('PAIR ct-secret '+ 'ab'.repeat(32)+'\n'));
assert.equal(lines.at(-1),'PAIR [sesión protegida]');
p.reset();p.push(bytes);assert.equal(values.length,2);
console.log('PASS: serial fragmented frames, CRLF, replay, invalid ranges, bounded buffer, secret redaction and restart');
// Tanque nulo: válido solo si el firmware declara sensorOk=false.
{
 const v2=[],e2=[];const q=new SerialParser(()=>{},s=>v2.push(s),e=>e2.push(e));
 const base={v:1,source:'wokwi',boot:'abcdef1234567890',seq:1,soil:50,tank:null,light:60,temp:null,humidity:null,pump:false,lamp:false,sensorOk:false,automatic:true,hivemq:true,ubidots:false};
 q.push(new TextEncoder().encode('CT_DATA '+JSON.stringify(base)+'\n'));
 assert.equal(v2.length,1);assert.equal(v2[0].tank,null);assert.equal(v2[0].ubidots,false);
 q.push(new TextEncoder().encode('CT_DATA '+JSON.stringify({...base,seq:2,sensorOk:true})+'\n'));
 assert.equal(v2.length,1);assert.match(e2.pop(),/rango/);
 const r=new SerialParser(x=>x,()=>{},()=>{});
 let shown='';const w=new SerialParser(x=>{shown=x;},()=>{},()=>{});
 w.push(new TextEncoder().encode('CT_READY\n'));assert.equal(shown,'CT_READY');
 console.log('PASS: tanque nulo con sensorOk=false y campos de red');
}
