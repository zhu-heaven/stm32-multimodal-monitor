import {sameOrigin,validPassword} from '../../../../lib/password-crypto';
import {allowAttempt,checkPassword,makeSession,sessionCookie} from '../../../../lib/password-auth';
export const dynamic='force-dynamic';
export async function POST(request:Request){
 if(!sameOrigin(request))return Response.json({error:'请从本站登录页面提交'},{status:403});
 try{
  if(!await allowAttempt(request))return Response.json({error:'尝试次数过多，请15分钟后重试'},{status:429,headers:{'Retry-After':'900'}});
  if(Number(request.headers.get('content-length'))>2048)return new Response(null,{status:413});
  const body=await request.json() as {password?:unknown};
  if(!validPassword(body.password)||!await checkPassword(body.password))return Response.json({error:'密码不正确'},{status:401,headers:{'Cache-Control':'no-store'}});
  return Response.json({ok:true},{headers:{'Set-Cookie':sessionCookie(await makeSession(body.password)),'Cache-Control':'no-store'}});
 }catch{return Response.json({error:'登录服务暂不可用，请稍后重试'},{status:503});}
}
