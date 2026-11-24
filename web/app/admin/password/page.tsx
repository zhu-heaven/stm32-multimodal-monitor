import {redirect} from 'next/navigation';
import {hasPasswordSession} from '../../../lib/password-auth';
import ChangePasswordForm from '../../../components/ChangePasswordForm';
export const dynamic='force-dynamic';
export default async function PasswordPage(){if(!await hasPasswordSession())redirect('/login');return <ChangePasswordForm/>;}
