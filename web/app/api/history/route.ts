import {history} from '../../../lib/onenet';
export const dynamic='force-dynamic';
export async function GET(request:Request){
  const p=new URL(request.url).searchParams;
  const start=Number(p.get('start')),end=Number(p.get('end')),offset=Number(p.get('offset')||0);
  if(!Number.isInteger(start)||!Number.isInteger(end)||start<1577808000000||end<=start||end-start>31*86400000||end>Date.now()+60000||!Number.isInteger(offset)||offset<0||offset>1000000)
    return Response.json({error:'请选择有效时间范围，单次最长31天'},{status:400});
  try{return Response.json(await history(p.get('id')||'displacement_change',start,end,offset),{headers:{'Cache-Control':'private, no-store'}});}
  catch(error){return Response.json({error:error instanceof Error?error.message:'历史数据读取失败'},{status:502});}
}
