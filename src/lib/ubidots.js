// Validación y almacenamiento del enlace público del dashboard de Ubidots.
// Solo se aceptan direcciones https de ubidots.com: el visor no incrusta sitios arbitrarios.
export const UBIDOTS_STORAGE_KEY='cultivatech.ubidots.dashboard';
export function isUbidotsUrl(value){
  try{
    const url=new URL(String(value).trim());
    const host=url.hostname.toLowerCase();
    return url.protocol==='https:'&&(host==='ubidots.com'||host.endsWith('.ubidots.com'))&&!url.username&&!url.password;
  }catch{return false;}
}
export function readSavedUbidotsUrl(storage=globalThis.localStorage){
  try{const value=storage?.getItem(UBIDOTS_STORAGE_KEY);return value&&isUbidotsUrl(value)?value:'';}catch{return '';}
}
export function saveUbidotsUrl(value,storage=globalThis.localStorage){
  try{if(value)storage?.setItem(UBIDOTS_STORAGE_KEY,value);else storage?.removeItem(UBIDOTS_STORAGE_KEY);}catch{/* almacenamiento no disponible: el enlace vale solo en esta sesión */}
}
