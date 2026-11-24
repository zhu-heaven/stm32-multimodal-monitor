import {env} from 'cloudflare:workers';
import {cookies} from 'next/headers';
import {digest,passwordHash,randomHex,constantEqual} from './password-crypto';
type Runtime=Cloudflare.Env & {SITE_PASSWORD_PEPPER?:string;PASSWORD_SETUP_ENABLED?:string};
const runtime=()=>env as Runtime;
export const SESSION_COOKIE='__Host-monitor_session';
export const sessionSeconds=7*86400;
export function authDb(){if(!env.DB)throw new Error('登录服务暂不可用');return env.DB;}
export function setupEnabled(){return runtime().PASSWORD_SETUP_ENABLED==='true';}
function pepper(){const p=runtime().SITE_PASSWORD_PEPPER;if(!p||p.length<32)throw new Error('登录服务尚未配置');return p;}
export async function configuredPassword(){return !!await authDb().prepare('SELECT id FROM site_password WHERE id=1').first();}
export async function hasPasswordSession(){
 const token=(await cookies()).get(SESSION_COOKIE)?.value;
 if(!token||!/^[a-f0-9]{64}$/.test(token))return false;
 const row=await authDb().prepare('SELECT expires FROM password_sessions WHERE token_hash=? AND expires>?').bind(await digest(token),Date.now()).first();return !!row;
}
export async function setInitialPassword(password:string){
 const salt=randomHex();const hash=await passwordHash(password,salt,pepper());
 const result=await authDb().prepare('INSERT OR IGNORE INTO site_password (id,salt,hash,created_at) VALUES (1,?,?,?)').bind(salt,hash,Date.now()).run();
 return result.meta.changes===1;
}
export async function checkPassword(password:string){
 const row=await authDb().prepare('SELECT salt,hash FROM site_password WHERE id=1').first<{salt:string;hash:string}>();
 if(!row)return false;return constantEqual(await passwordHash(password,row.salt,pepper()),row.hash);
}
export async function makeSession(password?:string){
 const row=await authDb().prepare('SELECT salt,hash FROM site_password WHERE id=1').first<{salt:string;hash:string}>();
 if(!row||(password!==undefined&&!constantEqual(await passwordHash(password,row.salt,pepper()),row.hash)))throw new Error('密码已变化，请重新登录');
 const token=randomHex();const now=Date.now();
 const results=await authDb().batch([
  authDb().prepare('DELETE FROM password_sessions WHERE expires<=?').bind(now),
  authDb().prepare('DELETE FROM password_attempts WHERE expires<=?').bind(now),
  authDb().prepare('INSERT INTO password_sessions (token_hash,expires) SELECT ?,? FROM site_password WHERE id=1 AND hash=? AND salt=?').bind(await digest(token),now+sessionSeconds*1000,row.hash,row.salt)
 ]);
 if(results[2].meta.changes!==1)throw new Error('密码已变化，请重新登录');
 return token;
}
export async function changePassword(current:string,next:string){
 const db=authDb();const row=await db.prepare('SELECT salt,hash FROM site_password WHERE id=1').first<{salt:string;hash:string}>();
 if(!row||!constantEqual(await passwordHash(current,row.salt,pepper()),row.hash))return false;
 const salt=randomHex(),hash=await passwordHash(next,salt,pepper());
 // D1 batch is transactional. Compare-and-swap prevents competing changes;
 // sessions are revoked only when this exact password update succeeds.
 const results=await db.batch([
  db.prepare('UPDATE site_password SET salt=?,hash=?,created_at=? WHERE id=1 AND salt=? AND hash=?').bind(salt,hash,Date.now(),row.salt,row.hash),
  db.prepare('DELETE FROM password_sessions WHERE EXISTS (SELECT 1 FROM site_password WHERE id=1 AND salt=? AND hash=?)').bind(salt,hash)
 ]);
 return results[0].meta.changes===1;
}
export async function removeSession(){const token=(await cookies()).get(SESSION_COOKIE)?.value;if(token)await authDb().prepare('DELETE FROM password_sessions WHERE token_hash=?').bind(await digest(token)).run();}
export function sessionCookie(token:string,clear=false){return `${SESSION_COOKIE}=${clear?'':token}; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=${clear?0:sessionSeconds}`;}
export async function allowAttempt(request:Request){
 const now=Date.now(),window=Math.floor(now/900000),ip=request.headers.get('cf-connecting-ip')||'unknown';
 const buckets=[{key:await digest(pepper()+':'+ip+':'+window),max:10},{key:'global:'+window,max:300}];
 const results=await authDb().batch(buckets.map(b=>authDb().prepare('INSERT INTO password_attempts (bucket,count,expires) VALUES (?,1,?) ON CONFLICT(bucket) DO UPDATE SET count=count+1 RETURNING count').bind(b.key,(window+2)*900000)));
 return results.every((r,i)=>Number((r.results?.[0] as {count?:number})?.count)<=buckets[i].max);
}
