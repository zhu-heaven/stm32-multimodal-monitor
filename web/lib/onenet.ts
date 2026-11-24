import { env } from 'cloudflare:workers';
import { metricById, normalizeReadings, normalizeHistory, type Snapshot } from './data';
type Runtime = Cloudflare.Env & {ONENET_ACCESS_KEY?:string;ONENET_API_TOKEN?:string;ONENET_PRODUCT_ID?:string;ONENET_DEVICE_NAME?:string};
const runtime=()=>env as Runtime;
export const config=()=>({productId:runtime().ONENET_PRODUCT_ID||'YOUR_PRODUCT_ID',deviceName:runtime().ONENET_DEVICE_NAME||'demo_device',configured:!!(runtime().ONENET_ACCESS_KEY||runtime().ONENET_API_TOKEN)});
async function authorization(){
  const e=runtime();if(e.ONENET_API_TOKEN)return e.ONENET_API_TOKEN;
  if(!e.ONENET_ACCESS_KEY)throw new Error('尚未配置 OneNET 服务端凭据');
  const resource=`products/${config().productId}`,version='2022-05-01',method='sha256',expires=String(Math.floor(Date.now()/1000)+3600);
  let keyBytes:Uint8Array<ArrayBuffer>;try{keyBytes=Uint8Array.from(atob(e.ONENET_ACCESS_KEY.trim()),c=>c.charCodeAt(0));}catch{throw new Error('OneNET 服务端凭据格式无效');}
  const key=await crypto.subtle.importKey('raw',keyBytes,{name:'HMAC',hash:'SHA-256'},false,['sign']);
  const signed=await crypto.subtle.sign('HMAC',key,new TextEncoder().encode(`${expires}\n${method}\n${resource}\n${version}`));
  const signature=btoa(String.fromCharCode(...new Uint8Array(signed)));
  return `version=${version}&res=${encodeURIComponent(resource)}&et=${expires}&method=${method}&sign=${encodeURIComponent(signature)}`;
}
async function request(path:string,params:Record<string,string>={}){
  const c=config();const url=new URL(`https://iot-api.heclouds.com/thingmodel/${path}`);
  for(const [k,v] of Object.entries({product_id:c.productId,device_name:c.deviceName,...params}))url.searchParams.set(k,v);
  const response=await fetch(url,{headers:{authorization:await authorization(),Accept:'application/json'},signal:AbortSignal.timeout(12000)});
  if(!response.ok)throw new Error(`OneNET 请求失败（HTTP ${response.status}）`);
  let body;try{body=await response.json() as {code?:number|string;msg?:string;data?:unknown;next_skip?:number};}catch{throw new Error('OneNET 返回了无法解析的响应');}
  if(Number(body.code)!==0)throw new Error(`OneNET 查询失败（代码 ${String(body.code??'unknown').slice(0,30)}），请检查凭据、权限或产品 API 配额。`);
  return body;
}
let cache:{at:number;data:Snapshot}|undefined;
export async function latest(force=false):Promise<Snapshot>{
  const c=config();if(!c.configured)return {...c,fetchedAt:Date.now(),readings:{}};
  if(!force&&cache&&Date.now()-cache.at<4500)return cache.data;
  // Workers I/O belongs to the current request. Cache only fully decoded data,
  // never an in-flight Promise, Response, or body shared with another request.
  const body=await request('query-device-property');
  const data:Snapshot={...c,fetchedAt:Date.now(),readings:normalizeReadings(body.data)};
  cache={at:Date.now(),data};
  return data;
}
export async function history(id:string,start:number,end:number,offset:number){
  if(!metricById[id]||!['float','double','int32','int64'].includes(metricById[id].type))throw new Error('不支持这个曲线指标');
  if(!config().configured)return {configured:false,points:[],hasMore:false,nextOffset:offset};
  const points=[];let next=offset,hasMore=false;
  // Bound each request; the UI can explicitly load older pages. Never imply
  // the newest page contains the whole selected date range.
  for(let page=0;page<6;page++){
    const body=await request('query-device-property-history',{identifier:id,start_time:String(start),end_time:String(end),sort:'2',limit:'100',offset:String(next)});
    const list=Array.isArray(body.data)?body.data:(body.data as {list?:unknown[]})?.list??[];
    points.push(...normalizeHistory(body.data));
    const serverNext=Number(body.next_skip);
    const advanced=Number.isInteger(serverNext)&&serverNext>next?serverNext:next+list.length;
    next=advanced;hasMore=list.length===100;
    if(!hasMore)break;
  }
  const unique=new Map(points.map(p=>[p.time,p.value]));
  return {configured:true,points:[...unique].map(([time,value])=>({time,value})).sort((a,b)=>a.time-b.time),hasMore,nextOffset:next};
}

export async function setDeviceProperties(params:Record<string,string|boolean>){
 const c=config();if(!c.configured)throw Error('尚未配置 OneNET');
 const response=await fetch('https://iot-api.heclouds.com/thingmodel/set-device-property',{method:'POST',headers:{authorization:await authorization(),'Content-Type':'application/json'},body:JSON.stringify({product_id:c.productId,device_name:c.deviceName,params}),signal:AbortSignal.timeout(18000)});
 if(!response.ok)throw Error('OneNET 控制请求失败（HTTP '+response.status+'）');
 const body=await response.json() as {code?:number;data?:{code?:number;msg?:string}};
 cache=undefined;
 if(Number(body.code)!==0)throw Error('OneNET 未完成控制（代码 '+String(body.code??'unknown')+'），请检查设备在线状态和属性权限');
 if(Number(body.data?.code)!==200)throw Error(body.data?.code===409?'设备拒绝手动操作：自动报警优先':'设备未确认执行（回执 '+String(body.data?.code??'缺失')+'）');
}
