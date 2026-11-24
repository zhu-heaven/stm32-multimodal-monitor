const encoder=new TextEncoder();
export const randomHex=(bytes=32)=>Array.from(crypto.getRandomValues(new Uint8Array(bytes)),v=>v.toString(16).padStart(2,'0')).join('');
export async function digest(value:string){return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',encoder.encode(value))),v=>v.toString(16).padStart(2,'0')).join('');}
export async function passwordHash(password:string,salt:string,pepper:string){
 const hmac=await crypto.subtle.importKey('raw',encoder.encode(pepper),{name:'HMAC',hash:'SHA-256'},false,['sign']);
 const prehash=await crypto.subtle.sign('HMAC',hmac,encoder.encode(password));
 const key=await crypto.subtle.importKey('raw',prehash,'PBKDF2',false,['deriveBits']);
 const result=await crypto.subtle.deriveBits({name:'PBKDF2',hash:'SHA-256',salt:encoder.encode(salt),iterations:100000},key,256);
 return Array.from(new Uint8Array(result),v=>v.toString(16).padStart(2,'0')).join('');
}
export function constantEqual(a:string,b:string){let diff=a.length^b.length;for(let i=0;i<Math.max(a.length,b.length);i++)diff|=(a.charCodeAt(i)||0)^(b.charCodeAt(i)||0);return diff===0;}
export function validPassword(password:unknown):password is string{return typeof password==='string'&&password.length>=12&&password.length<=128;}
export function sameOrigin(request:Request){return request.headers.get('origin')===new URL(request.url).origin;}
