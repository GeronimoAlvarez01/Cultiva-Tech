import {WokwiClient} from './vendor/wokwi/wokwi-client.js';
import {MessagePortTransport} from './vendor/wokwi/message-port-transport.js';
export const WOKWI_PROJECT='https://wokwi.com/projects/479069244965768193';
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
    iframe.src='https://wokwi.com/experimental/embed?client_id=wokwi_client_omf4ejkz6n6twj7d3x2eqmyp';
  }
  async request(p){let timer;try{return await Promise.race([p,new Promise((_,reject)=>timer=setTimeout(()=>reject(Error('Wokwi no respondió dentro del plazo.')),40000))]);}finally{clearTimeout(timer);}}
  async load(){
    this.status('loading','Cargando circuito y firmware compilado…');
    const [diagram,firmware]=await Promise.all([fetch('wokwi/diagram.json'),fetch('wokwi/firmware.bin')]);
    if(!diagram.ok||!firmware.ok)throw Error('No se encontraron los archivos del prototipo.');
    await this.request(this.client.serialMonitorListen());
    const circuit=await diagram.json();
    circuit.serialMonitor={...circuit.serialMonitor,display:'never',collapse:true};
    await this.request(this.client.fileUpload('diagram.json',JSON.stringify(circuit)));
    await this.request(this.client.fileUpload('firmware.bin',new Uint8Array(await firmware.arrayBuffer())));
    this.ready=true;clearTimeout(this.timer);this.status('ready','Circuito conectado · listo para iniciar');
  }
  async start(){if(!this.ready)throw Error('Espera a que termine de cargar el circuito.');this.status('starting','Arrancando ESP32…');await this.request(this.client.simStart({firmware:'firmware.bin'}));this.started=true;this.status('running','Wokwi ejecutándose · esperando lectura');}
  async pause(){await this.request(this.client.simPause());this.status('paused','Simulación pausada');}
  async resume(){await this.request(this.client.simResume());this.status('running','Simulación reanudada');}
  async restart(){await this.request(this.client.simRestart());this.status('running','ESP32 reiniciado');}
  async write(line){await this.request(this.client.serialMonitorWrite(new TextEncoder().encode(line+'\n')));}
  destroy(){clearTimeout(this.timer);window.removeEventListener('message',this.listener);this.client?.transport.port.close();}
}
