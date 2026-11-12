import assert from 'node:assert/strict';
import {SIM_GROUPS,nextSimState,simAction,SIM_NORMAL} from '../src/simulador.js';
// Cada comando de los botones debe existir en el firmware (wokwi/final/sketch.ino).
import {readFileSync} from 'node:fs';
const sketch=readFileSync(new URL('../wokwi/final/sketch.ino',import.meta.url),'utf8');
for(const g of SIM_GROUPS)for(const a of [g.on,g.off])assert.ok(sketch.includes('"'+a.cmd+'"'),'el firmware no conoce '+a.cmd);
assert.ok(sketch.includes('"'+SIM_NORMAL.cmd+'"'));
let s={};
assert.equal(simAction(s,'tank').cmd,'SIM_TANQUE_VACIO');
s=nextSimState(s,'tank');assert.equal(s.tank,true);assert.equal(simAction(s,'tank').cmd,'SIM_TANQUE_LLENO');
s=nextSimState(s,'sonic');assert.equal(s.sonic,true);assert.equal(s.tank,false,'una falla del HC-SR04 cancela «tanque vacío»');
s=nextSimState(s,'tank');assert.equal(s.tank,true);assert.equal(s.sonic,false,'vaciar el tanque repara la falla simulada');
s=nextSimState(s,'dht');s=nextSimState(s,'temp');assert.equal(s.dht,false);
console.log('PASS: botones del simulador alternan, se liberan entre sí y usan comandos que el firmware reconoce');
