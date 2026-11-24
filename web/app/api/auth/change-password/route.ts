import {sameOrigin,validPassword} from '../../../../lib/password-crypto';
import {hasPasswordSession,allowAttempt,changePassword,sessionCookie} from '../../../../lib/password-auth';
export const dynamic='force-dynamic';
export async function POST(request:Request){
 if(!sameOrigin(request))return Response.json({error:'请从本站修改密码页面提交'},{status:403});
 if(!await hasPasswordSession())return Response.json({error:'管理员登录已失效，请重新登录'},{status:401});
 try{
  if(!await allowAttempt(request))return Response.json({error:'尝试次数过多，请15分钟后重试'},{status:429,headers:{'Retry-After':'900'}});
  const text=await request.text();if(text.length>4096)return Response.json({error:'请求内容过长'},{status:413});
  let body:{currentPassword?:unknown;newPassword?:unknown;confirm?:unknown};
  try{body=JSON.parse(text);}catch{return Response.json({error:'请求格式不正确'},{status:400});}
  if(!body||!validPassword(body.currentPassword)||!validPassword(body.newPassword)||body.newPassword!==body.confirm)return Response.json({error:'密码需为12～128位，两次新密码须一致'},{status:400});
  if(body.currentPassword===body.newPassword)return Response.json({error:'新密码需要与当前密码不同'},{status:400});
  if(!await changePassword(body.currentPassword,body.newPassword))return Response.json({error:'当前密码不正确，或已被其他管理员修改，请核对后重试'},{status:400});
  return Response.json({ok:true},{headers:{'Set-Cookie':sessionCookie('',true),'Cache-Control':'no-store'}});
 }catch{return Response.json({error:'暂时无法修改密码，请稍后重试'},{status:503});}
}
