import assert from 'node:assert/strict';
import {DemoModel} from '../src/lib/DemoModel.js';
const m=new DemoModel();m.inputs.soil=39;assert.equal(m.tick(1).pump,true);
m.inputs.soil=59;assert.equal(m.tick(2).pump,true);
m.inputs.soil=60;assert.equal(m.tick(3).pump,false);
m.inputs.soil=50;assert.equal(m.tick(4).pump,false);
m.inputs.soil=20;m.inputs.tank=14;assert.equal(m.tick(5).pump,false);
m.inputs.tank=19;assert.equal(m.tick(6).pump,false);
m.inputs.tank=20;assert.equal(m.tick(7).pump,true);
m.inputs.dhtFail=true;{const f=m.tick(8);assert.equal(f.pump,false);assert.equal(f.temp,null);assert.equal(f.sensorOk,false);assert.ok(f.alarmMask&8);}
m.inputs.dhtFail=false;m.inputs.light=24;assert.equal(m.tick(9).lamp,true);
m.inputs.light=39;assert.equal(m.tick(10).lamp,true);
m.inputs.light=40;assert.equal(m.tick(11).lamp,false);
m.inputs.light=60;m.inputs.soil=10;m.tick(12);{const r=m.tick(61020);assert.equal(r.reason,'TIEMPO_MAXIMO');assert.ok(r.alarmMask&16);}
m.inputs.sonicFail=true;{const f=m.tick(61030);assert.equal(f.tank,null);assert.equal(f.sensorOk,false);}
m.inputs.sonicFail=false;m.inputs.temp=35;assert.ok(m.tick(61040).alarmMask&1);
console.log('PASS: límites de la demostración');
