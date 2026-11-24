import {latest,config} from '../../../lib/onenet';
export const dynamic='force-dynamic';
export async function GET(){
  try{return Response.json(await latest(),{headers:{'Cache-Control':'private, no-store'}});}
  catch(error){return Response.json({...config(),fetchedAt:Date.now(),readings:{},error:error instanceof Error?error.message:'读取失败'},{status:502,headers:{'Cache-Control':'no-store'}});}
}
