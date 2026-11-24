import {redirect} from 'next/navigation';
import PasswordForm from '../../components/PasswordForm';
import {hasPasswordSession} from '../../lib/password-auth';
export const dynamic='force-dynamic';
export default async function Login(){if(await hasPasswordSession())redirect('/#admin');return <PasswordForm/>;}
