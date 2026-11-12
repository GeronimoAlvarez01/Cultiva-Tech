import {WokwiClient} from './wokwi-client/wokwi-client.js';
import {MessagePortTransport} from './wokwi-client/message-port-transport.js';
import {WOKWI_EMBED_URL} from '../config.js';
export class WokwiBridge {
  constructor(iframe,{status,serial}){
    this.iframe=iframe;this.status=status;this.serial=serial;this.ready=false;this.started=false;
    this.listener=e=>{
      if(e.origin!=='https://wokwi.com'||e.source!==iframe.contentWindow||!(e.data?.port instanceof MessagePort)||this.client)return;
      const client=this.client=new WokwiClient(new MessagePortTransport(e.data.port));
      client.addEventListener('serial-monitor:data',e=>this.serial(e.detail.bytes));
      client.addEventListener('wokwi:connected',()=>this.load().catch(e=>this.status('error',e.message)));
    };
    window.addEventListener('message',this.listener);
    this.timer=setTimeout(()=>{if(!this.ready)this.status('error','Wokwi tarda en responder. Puedes reintentar o abrir el proyecto guardado.');},45000);
    iframe.src=WOKWI_EMBED_URL;
  }
  async request(p){let timer;try{return await Promise.race([p,new Promise((_,reject)=>timer=setTimeout(()=>reject(Error('Wokwi no respondió dentro del plazo.')),40000))]);}finally{clearTimeout(timer);}}
  async load(){
    this.status('loading','Cargando circuito y firmware compilado…');
    const [diagram,firmware]=await Promise.all([fetch(import.meta.env.BASE_URL+'wokwi/diagram.json'),fetch(import.meta.env.BASE_URL+'wokwi/firmware.bin')]);
    if(!diagram.ok)throw Error('No se encontró wokwi/diagram.json. Ejecuta npm run sync:wokwi.');
    // Vite devuelve index.html con código 200 cuando el archivo no existe: se comprueba el contenido.
    const binary=firmware.ok?new Uint8Array(await firmware.arrayBuffer()):new Uint8Array(0);
    const looksHtml=binary.length<4096||binary[0]===0x3c||(firmware.headers.get('content-type')||'').includes('text/html');
    if(looksHtml)throw Error('Falta public/wokwi/firmware.bin (o no es un binario). Compila wokwi/final/sketch.ino en Wokwi, descarga el firmware con F1 → «Download Compiled Firmware», guárdalo en wokwi/final/firmware.bin y ejecuta npm run sync:wokwi.');
    await this.request(this.client.serialMonitorListen());
    const circuit=await diagram.json();
    circuit.serialMonitor={...circuit.serialMonitor,display:'never',collapse:true};
    await this.request(this.client.fileUpload('diagram.json',JSON.stringify(circuit)));
    await this.request(this.client.fileUpload('firmware.bin',binary));
    if(this.destroyed)return;this.ready=true;clearTimeout(this.timer);this.status('ready','Circuito conectado · listo para iniciar');
  }
  async start(){if(!this.ready)throw Error('Espera a que termine de cargar el circuito.');this.status('starting','Arrancando ESP32…');await this.request(this.client.simStart({firmware:'firmware.bin'}));this.started=true;this.status('running','Wokwi ejecutándose · esperando lectura');}
  async pause(){await this.request(this.client.simPause());this.status('paused','Simulación pausada');}
  async resume(){await this.request(this.client.simResume());this.status('running','Simulación reanudada');}
  async restart(){await this.request(this.client.simRestart());this.status('running','ESP32 reiniciado');}
  async write(line){await this.request(this.client.serialMonitorWrite(new TextEncoder().encode(line+'\n')));}
  destroy(){this.destroyed=true;clearTimeout(this.timer);window.removeEventListener('message',this.listener);this.client?.transport.port.close();}
}
