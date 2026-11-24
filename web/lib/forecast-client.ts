import {forecastUrl,forecastPeriod,parseForecast,retryDelay,type RainForecast} from './forecast-data';
const key='changsha-rain-centered-v2';
type Saved={raw?:unknown;at?:number;retryAt?:number};
function read():Saved{try{const value=JSON.parse(localStorage.getItem(key)||'{}');return value&&typeof value==='object'?value:{};}catch{return {};}}
function save(value:Saved){try{localStorage.setItem(key,JSON.stringify(value));}catch{/* Storage is optional in private browsing. */}}
export async function browserForecast(refresh=false):Promise<RainForecast>{
 const now=Date.now(),stored=read();let cached:RainForecast|undefined;
 try{if(typeof stored.at==='number'&&stored.at<=now&&now-stored.at<3600000){cached=parseForecast(stored.raw,stored.at);if(cached.periodStart!==forecastPeriod(now).dates[0])cached=undefined;}}catch{/* Reject corrupt or expired cached data. */}
 if(cached&&now-cached.fetchedAt<(refresh?60000:600000))return cached;
 if(stored.retryAt&&stored.retryAt>now){if(cached)return {...cached,stale:true,warning:'天气接口重试冷却中，显示本机一小时内缓存。'};throw Error(`天气接口暂不可用，将于北京时间 ${new Date(stored.retryAt).toLocaleTimeString('zh-CN',{timeZone:'Asia/Shanghai',hour12:false})} 后允许重试；请勿连续刷新。`);}
 try{
  const response=await fetch(forecastUrl(now),{signal:AbortSignal.timeout(15000),credentials:'omit',headers:{Accept:'application/json'}});
  if(!response.ok){const retryAt=now+retryDelay(response.headers.get('Retry-After'),now,response.status===429?1800000:300000);save({...stored,retryAt});throw Error(`天气接口暂不可用（HTTP ${response.status}），已暂停重复请求，请稍后再试。`);}
  const raw:unknown=await response.json(),result=parseForecast(raw,now);save({raw,at:now});return result;
 }catch(e){if((read().retryAt||0)<=now)save({...stored,retryAt:now+300000});if(cached)return {...cached,stale:true,warning:'本次更新失败，暂显示本机一小时内的有效预报缓存。'};throw e;}
}
