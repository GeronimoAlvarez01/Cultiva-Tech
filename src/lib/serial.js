import {validateTelemetry} from './protocol.js';
// Oculta secretos que se escriban por el serial (token de Ubidots, URL de webhook, chat_id de Telegram).
export function redactSerialInput(line){return String(line).replace(/^(UBIDOTS_TOKEN|WEBHOOK|TELEGRAM_CHAT)\s+.*$/i,'$1 [oculto]');}
export class SerialParser {
  constructor(onLine,onData,onError){this.onLine=onLine;this.onData=onData;this.onError=onError;this.buffer='';this.discard=false;this.seen=new Map();this.decoder=new TextDecoder();}
  reset(){this.buffer='';this.discard=false;this.seen.clear();this.decoder=new TextDecoder();}
  push(bytes){
    const text=this.decoder.decode(new Uint8Array(bytes),{stream:true});
    for(const char of text){
      if(char==='\n'){
        if(!this.discard)this.line(this.buffer.replace(/\r$/,''));
        this.buffer='';this.discard=false;
      }else if(!this.discard){this.buffer+=char;if(this.buffer.length>4096){this.buffer='';this.discard=true;this.onError('Línea serial demasiado larga');}}
    }
  }
  line(line){
    if(!line)return;
    // Never display a pairing key, including firmware echoes.
    this.onLine(redactSerialInput(line.replace(/PAIR\s+[A-Za-z0-9-]+\s+[a-fA-F0-9]{64}/g,'PAIR [sesión protegida]')));
    if(!line.startsWith('CT_DATA '))return;
    try{
      const s=validateTelemetry(JSON.parse(line.slice(8)));
      if(s.seq<=(this.seen.get(s.boot)||0))throw Error('Muestra serial repetida');
      this.seen.set(s.boot,s.seq);if(this.seen.size>8)this.seen.delete(this.seen.keys().next().value);
      this.onData(s);
    }catch(e){this.onError(e.message);}
  }
}
