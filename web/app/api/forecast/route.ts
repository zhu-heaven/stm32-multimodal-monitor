import {rainForecast,ForecastUnavailable} from '../../../lib/forecast';
export const dynamic='force-dynamic';
export async function GET(request:Request){
 try{return Response.json(await rainForecast(new URL(request.url).searchParams.get('refresh')==='1'),{headers:{'Cache-Control':'no-store'}});}
 catch(e){return Response.json({error:e instanceof Error?e.message:'降水预报获取失败',retryAt:e instanceof ForecastUnavailable?e.retryAt:undefined},{status:e instanceof ForecastUnavailable?503:502,headers:{'Cache-Control':'no-store'}});}
}
