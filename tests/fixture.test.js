import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {SerialParser} from '../src/lib/serial.js';
// Las líneas CT_DATA reales impresas por wokwi/final/sketch.ino (compilado en PC con stubs) deben pasar el validador del visor.
const text=readFileSync(new URL('./firmware/ct_data.log',import.meta.url),'utf8');
const vals=[],errs=[];const p=new SerialParser(()=>{},s=>vals.push(s),e=>errs.push(e));
p.push(new TextEncoder().encode(text));
const n=text.split('\n').filter(l=>l.startsWith('CT_DATA ')).length;
assert.ok(n>=2,'el registro debe tener varias tramas');
assert.deepEqual(errs.filter(e=>!/repetida/.test(e)),[]);
assert.equal(vals.length,n);
console.log('PASS: '+n+' tramas CT_DATA del firmware validadas por el visor');
