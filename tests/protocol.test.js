import assert from 'node:assert/strict';
import {validateTelemetry} from '../src/lib/protocol.js';
const ok={v:1,source:'wokwi',boot:'aabbccdd00112233',seq:1,soil:30,tank:60,light:70,temp:24,humidity:65,pump:true,lamp:false,sensorOk:true,automatic:true};
assert.equal(validateTelemetry(ok).soil,30);
for(const bad of [{...ok,tank:101},{...ok,soil:-1},{...ok,v:2},{...ok,source:'x'},{...ok,boot:'zz'},{...ok,seq:0},{...ok,pump:'si'},{...ok,temp:200},{...ok,tank:null}])assert.throws(()=>validateTelemetry(bad));
assert.equal(validateTelemetry({...ok,tank:null,sensorOk:false}).tank,null);
assert.equal(validateTelemetry({...ok,temp:null,humidity:null}).temp,null);
console.log('PASS: validación de tramas (rangos, estados, tanque nulo solo con sensor inválido)');
