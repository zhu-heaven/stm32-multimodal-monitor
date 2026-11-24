import {redirect} from 'next/navigation';
import {requireChatGPTUser} from '../chatgpt-auth';
import PasswordForm from '../../components/PasswordForm';
import {setupEnabled,configuredPassword} from '../../lib/password-auth';
export const dynamic='force-dynamic';
export default async function Setup(){
 if(!setupEnabled()||await configuredPassword())redirect('/login');
 await requireChatGPTUser('/password-setup');return <PasswordForm setup/>;
}
