import {sameOrigin} from '../../../../lib/password-crypto';
import {removeSession,sessionCookie} from '../../../../lib/password-auth';
export async function POST(request:Request){
 if(!sameOrigin(request))return new Response(null,{status:403});
 await removeSession();return Response.json({ok:true},{headers:{'Set-Cookie':sessionCookie('',true),'Cache-Control':'no-store'}});
}
