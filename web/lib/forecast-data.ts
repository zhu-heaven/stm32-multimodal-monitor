export type RainPoint={time:number;amount:number|null};
export type RainForecast={city:string;source:string;timezone:string;periodStart:string;periodEnd:string;fetchedAt:number;daily:RainPoint[];hourly:RainPoint[];total:number;average:number;stale?:boolean;warning?:string};
const dayMs=86400000,offset=28800000;
export function forecastUrl(now=Date.now()){
 const p=forecastPeriod(now),url=new URL('https://api.open-meteo.com/v1/forecast');
 for(const [k,v] of Object.entries({latitude:'28.2282',longitude:'112.9388',hourly:'precipitation',daily:'precipitation_sum',precipitation_unit:'mm',timezone:'Asia/Shanghai',start_date:p.dates[0],end_date:p.dates[2]}))url.searchParams.set(k,v);
 return url.toString();
}
export function retryDelay(header:string|null,now=Date.now(),minimum=1800000){
 const seconds=header===null?NaN:Number(header),date=header?Date.parse(header):NaN;
 return Math.max(minimum,Number.isFinite(seconds)?seconds*1000:Number.isFinite(date)?date-now:0);
}
export function forecastPeriod(now=Date.now()){
 const start=Math.floor((now+offset)/dayMs)*dayMs-dayMs-offset;
 const dates=Array.from({length:3},(_,i)=>new Date(start+i*dayMs+offset).toISOString().slice(0,10));
 return {start,end:start+3*dayMs,dates};
}
function amounts(times:unknown,values:unknown,label:string){
 if(!Array.isArray(times)||!Array.isArray(values)||times.length!==values.length)throw Error(`${label}预报数据结构不完整`);
 const map=new Map<string,number|null>();
 for(let i=0;i<times.length;i++){
  const t=times[i],v=values[i];if(typeof t!=='string'||map.has(t))throw Error(`${label}预报日期无效`);
  if(v!==null&&(typeof v!=='number'||!Number.isFinite(v)||v<0))throw Error(`${label}降水值无效`);
  map.set(t,v);
 }return map;
}
export function parseForecast(input:unknown,now=Date.now()):RainForecast{
 const b=input as {timezone?:unknown;utc_offset_seconds?:unknown;daily_units?:{precipitation_sum?:unknown};hourly_units?:{precipitation?:unknown};daily?:{time?:unknown;precipitation_sum?:unknown};hourly?:{time?:unknown;precipitation?:unknown}};
 if(!b||b.timezone!=='Asia/Shanghai'||b.utc_offset_seconds!==28800)throw Error('预报时区不是北京时间');
 if(b.daily_units?.precipitation_sum!=='mm'||b.hourly_units?.precipitation!=='mm')throw Error('降水预报单位不是毫米');
 const p=forecastPeriod(now),days=amounts(b.daily?.time,b.daily?.precipitation_sum,'逐日'),hours=amounts(b.hourly?.time,b.hourly?.precipitation,'逐小时');
 const daily=p.dates.map((date,i)=>{const amount=days.get(date);if(amount===undefined||amount===null)throw Error('昨天、今天、明天的逐日降水缺失');return {time:p.start+i*dayMs,amount};});
 const hourly=Array.from({length:72},(_,i)=>{const time=p.start+i*3600000,key=new Date(time+offset).toISOString().slice(0,16);return {time,amount:hours.get(key)??null};});
 if(!hourly.some(h=>h.amount!==null))throw Error('三日逐小时降水缺失');
 const total=Math.round(daily.reduce((sum,d)=>sum+d.amount,0)*100)/100;
 return {city:'长沙',source:'Open-Meteo',timezone:'Asia/Shanghai',periodStart:p.dates[0],periodEnd:p.dates[2],fetchedAt:now,daily,hourly,total,average:Math.round(total/3*100)/100};
}
