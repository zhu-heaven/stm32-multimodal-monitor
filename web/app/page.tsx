import Dashboard from '../components/Dashboard';
import {hasPasswordSession} from '../lib/password-auth';
export const dynamic='force-dynamic';
export default async function Home(){
 const isAdmin=await hasPasswordSession();
 return <Dashboard userName={isAdmin?'管理员':'游客'} isAdmin={isAdmin}/>;
}
