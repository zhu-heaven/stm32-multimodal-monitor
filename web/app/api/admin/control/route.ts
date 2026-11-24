import {hasPasswordSession,authDb} from '../../../../lib/password-auth';
import {sameOrigin} from '../../../../lib/password-crypto';
import {runControl,ControlError} from '../../../../lib/device-control';
export const dynamic='force-dynamic';
export async function POST(request:Request){
 if(!await hasPasswordSession())return Response.json({error:'仅管理员可以修改规则或控制蜂鸣器'},{status:401});
 if(!sameOrigin(request))return Response.json({error:'请从本站管理页面提交'},{status:403});
 try{const text=await request.text();if(text.length>4096)throw new ControlError('请求过大');const body=JSON.parse(text) as {kind?:unknown};if(body.kind!=='config'&&body.kind!=='buzzer')throw new ControlError('不支持的操作');return Response.json(await runControl(body.kind,body),{headers:{'Cache-Control':'no-store'}});}
 catch(e){return Response.json({error:e instanceof Error?e.message:'控制失败'},{status:e instanceof ControlError?e.status:502});}
}
export async function GET(){
 if(!await hasPasswordSession())return Response.json({error:'仅管理员可查看操作记录'},{status:401});
 const rows=await authDb().prepare('SELECT id,kind,revision,status,message,created_at FROM control_commands ORDER BY created_at DESC LIMIT 10').all();return Response.json({commands:rows.results},{headers:{'Cache-Control':'no-store'}});
}
