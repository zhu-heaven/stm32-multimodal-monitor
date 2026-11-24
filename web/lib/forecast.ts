import {env} from 'cloudflare:workers';
import {forecastPeriod,forecastUrl,retryDelay,parseForecast,type RainForecast} from './forecast-data';
export class ForecastUnavailable extends Error{constructor(public retryAt:number){super('天气服务正在限流或暂时不可用，已暂停重复请求');}}
async function cooldown(){if(!env.DB)return 0;const row=await env.DB.prepare('SELECT fetched_at FROM forecast_cache WHERE id=?').bind('changsha-rain-retry').first<{fetched_at:number}>();return row?.fetched_at||0;}
async function pause(until:number){if(env.DB)await env.DB.prepare('INSERT INTO forecast_cache (id,payload,fetched_at) VALUES (?,?,?) ON CONFLICT(id) DO UPDATE SET fetched_at=excluded.fetched_at').bind('changsha-rain-retry','{}',until).run();}
async function readCache(now:number){
 if(!env.DB)return null;
 const row=await env.DB.prepare('SELECT payload,fetched_at FROM forecast_cache WHERE id=?').bind('changsha-rain-centered-v2').first<{payload:string;fetched_at:number}>();
 if(!row)return null;
 try{const c=JSON.parse(row.payload) as RainForecast;const p=forecastPeriod(now);return c.periodStart===p.dates[0]&&c.periodEnd===p.dates[2]&&c.fetchedAt===row.fetched_at&&now>=c.fetchedAt&&now-c.fetchedAt<3600000?c:null;}catch{return null;}
}
async function load(refresh:boolean):Promise<RainForecast>{
 const now=Date.now(),cached=await readCache(now);
 if(cached&&now-cached.fetchedAt<(refresh?60000:600000))return cached;
 const retryAt=await cooldown();
 if(retryAt>now){if(cached)return {...cached,stale:true,warning:'天气服务重试冷却中，暂显示一小时内的有效缓存。'};throw new ForecastUnavailable(retryAt);}
 try{
  const response=await fetch(forecastUrl(now),{signal:AbortSignal.timeout(12000),headers:{Accept:'application/json'}});
  if(!response.ok){const until=now+retryDelay(response.headers.get('Retry-After'),now,response.status===429?1800000:300000);await pause(until);throw new ForecastUnavailable(until);}
  const result=parseForecast(await response.json(),now);
  if(env.DB)await env.DB.prepare('INSERT INTO forecast_cache (id,payload,fetched_at) VALUES (?,?,?) ON CONFLICT(id) DO UPDATE SET payload=excluded.payload,fetched_at=excluded.fetched_at WHERE excluded.fetched_at>=forecast_cache.fetched_at').bind('changsha-rain-centered-v2',JSON.stringify(result),now).run();
  return result;
 }catch(e){if(!(e instanceof ForecastUnavailable))await pause(now+300000);if(cached)return {...cached,stale:true,warning:'本次更新失败，暂显示一小时内的有效缓存；请稍后刷新。'};throw e;}
}
// Persist parsed forecast data and retry times in D1; each request owns its I/O.
export async function rainForecast(refresh=false){return load(refresh);}
