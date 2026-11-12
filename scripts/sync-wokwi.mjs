// Copia wokwi/final → public/wokwi para que el visor ejecute el MISMO circuito y firmware.
// Este comando NO compila Arduino: firmware.bin debe descargarse de Wokwi (ver README).
import {cp,mkdir,rm,access} from 'node:fs/promises';
await rm('public/wokwi',{recursive:true,force:true});
await mkdir('public/wokwi',{recursive:true});
for(const f of ['diagram.json','libraries.txt','sketch.ino','CONEXIONES.md','firmware.bin','firmware.elf']){
  try{await access('wokwi/final/'+f);await cp('wokwi/final/'+f,'public/wokwi/'+f);console.log('copiado',f);}catch{/* opcional */}
}
try{await access('wokwi/final/firmware.bin');}catch{
  console.warn('\nATENCIÓN: falta wokwi/final/firmware.bin.\n1) Abre wokwi/final en Wokwi (sketch.ino + diagram.json + libraries.txt) y pulsa Play.\n2) F1 → «Download Compiled Firmware» y guarda el archivo como wokwi/final/firmware.bin.\n3) Ejecuta de nuevo: npm run sync:wokwi');
}
