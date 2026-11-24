import definitions from './metrics.json';
export type Metric = {id:string;name:string;type:string;unit?:string|null;description:string};
export const metrics = definitions as Metric[];
export const numericMetrics = metrics.filter(m=>['float','double','int32','int64'].includes(m.type));
export const metricById = Object.fromEntries(metrics.map(m=>[m.id,m]));
export type Reading = {id:string;value:number|string|boolean|null;time:number};
export type Snapshot = {configured:boolean;productId:string;deviceName:string;fetchedAt:number;readings:Record<string,Reading>;error?:string};
export type HistoryPoint = {time:number;value:number};
export function parseValue(input:unknown):Reading['value'] {
  if(input===null||input===undefined)return null;
  if(typeof input==='number')return Number.isFinite(input)?input:null;
  if(typeof input==='boolean')return input;
  if(typeof input==='object'&&'value' in input)return parseValue((input as {value:unknown}).value);
  const s=String(input);try {const v=JSON.parse(s);if(typeof v!=='string'||v!==s)return parseValue(v);}catch{}
  return s;
}
export function timestamp(input:unknown):number {
  const n=Number(input);if(Number.isFinite(n)&&n>0)return n<1e12?n*1000:n;
  if(typeof input==='string'){const t=Date.parse(input);if(Number.isFinite(t))return t;}
  return 0;
}
export function normalizeReadings(data:unknown):Record<string,Reading> {
  const list=Array.isArray(data)?data:(data as {list?:unknown[]})?.list??[];
  const out:Record<string,Reading>={};
  for(const item of list){const r=item as Record<string,unknown>;const id=String(r.identifier??'');if(!metricById[id])continue;
    out[id]={id,value:parseValue(r.value),time:timestamp(r.time)};}
  return out;
}
export function normalizeHistory(data:unknown):HistoryPoint[] {
  const list=Array.isArray(data)?data:(data as {list?:unknown[]})?.list??[];
  const points=new Map<number,number>();
  for(const row of list){const r=row as Record<string,unknown>;const v=parseValue(r.value),t=timestamp(r.time);
    if(typeof v==='number'&&t>0)points.set(t,v);}
  return [...points].map(([time,value])=>({time,value})).sort((a,b)=>a.time-b.time);
}
export function formatValue(id:string,value:Reading['value']|undefined){
  if(value===undefined||value===null)return '—';
  if(typeof value==='boolean')return value?'开启':'关闭';
  if(typeof value!=='number')return value;
  const digits=['displacement_mm','displacement_avg','displacement_change','tilt_angle','roll_angle','pitch_angle','tilt_change','risk_score','tilt_rate'].includes(id)?2:
    ['rain_wetness','soil_moisture','chip_temp','weather_temp','weather_wind','forecast_rain_3d','forecast_rain_avg','gyro_x','gyro_y','gyro_z'].includes(id)?1:0;
  return value.toFixed(digits);
}
export function unit(id:string){return metricById[id]?.unit==='mm/d'?'mm/日':metricById[id]?.unit??'';}
export function beijingTime(time:number,seconds=true){return time?new Intl.DateTimeFormat('zh-CN',{timeZone:'Asia/Shanghai',year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',...(seconds?{second:'2-digit'}:{}),hour12:false}).format(time):'暂无上传记录';}
export function ageLabel(time:number,now=Date.now()) {if(!time)return '尚无数据';const seconds=Math.max(0,Math.floor((now-time)/1000));return seconds<60?`${seconds} 秒前`:seconds<3600?`${Math.floor(seconds/60)} 分钟前`:`${Math.floor(seconds/3600)} 小时前`;}
