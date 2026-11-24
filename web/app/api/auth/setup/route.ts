import {getChatGPTUser} from '../../../chatgpt-auth';
import {sameOrigin,validPassword} from '../../../../lib/password-crypto';
import {setupEnabled,configuredPassword,setInitialPassword,makeSession,sessionCookie} from '../../../../lib/password-auth';
export const dynamic='force-dynamic';
export async function POST(request:Request){
 if(!sameOrigin(request))return new Response(null,{status:403});
 if(!setupEnabled()||!await getChatGPTUser())return new Response(null,{status:403});
 try{
  if(await configuredPassword())return Response.json({error:'密码已经设置，初始化入口已关闭'},{status:409});
  if(Number(request.headers.get('content-length'))>2048)return new Response(null,{status:413});
  const body=await request.json() as {password?:unknown;confirm?:unknown};
  if(!validPassword(body.password)||body.password!==body.confirm)return Response.json({error:'请输入12～128位密码，且两次输入一致'},{status:400});
  if(!await setInitialPassword(body.password))return Response.json({error:'密码已经设置'},{status:409});
  return Response.json({ok:true},{headers:{'Set-Cookie':sessionCookie(await makeSession()),'Cache-Control':'no-store'}});
 }catch{return Response.json({error:'暂时无法设置密码，请稍后重试'},{status:503});}
}
