import {authDb} from './password-auth';
import {latest,setDeviceProperties} from './onenet';
import {decodeRisk,encodeRisk,validateRisk,type RiskConfig} from './risk-config';
export class ControlError extends Error{constructor(message:string,public status=400){super(message);}}
function fresh(s:Awaited<ReturnType<typeof latest>>){const t=s.readings.risk_level?.time??0;if(!t||Date.now()-t>15000)throw new ControlError('设备当前数据过期，暂不下发控制',409);}
export async function runControl(kind:'config'|'buzzer',input:unknown){
 const snapshot=await latest(true);fresh(snapshot);
 const actual=decodeRisk(snapshot.readings.risk_config?.value);
 const body=input as {config?:unknown;baseRevision?:number;on?:boolean};
 let target:Record<string,string|boolean>,revision=0;
 if(kind==='config'){
  if(!actual)throw new ControlError('设备尚未上报可配置规则，请先完成固件和物模型更新',409);
  if(body.baseRevision!==actual.revision)throw new ControlError('规则已变化，请重新载入后再编辑',409);
  validateRisk(body.config);
 }else{
  if(typeof body.on!=='boolean')throw new ControlError('开关值必须为布尔值');
  if(['预警','危险','确认中'].includes(String(snapshot.readings.risk_level?.value)))throw new ControlError('自动报警正在生效，手动试鸣不能覆盖自动报警',409);
 }
 const db=authDb(),now=Date.now();
 await db.prepare('INSERT OR IGNORE INTO control_locks (id,revision,busy_until) VALUES (?,0,0)').bind(kind).run();
 const lock=await db.prepare('UPDATE control_locks SET busy_until=?,revision=MAX(revision,?)+1 WHERE id=? AND busy_until<? RETURNING revision').bind(now+45000,actual?.revision??0,kind,now).first<{revision:number}>();
 if(!lock)throw new ControlError('已有控制操作正在处理，请稍后重试',409);
 revision=lock.revision;
 if(kind==='config')target={risk_config:encodeRisk({...validateRisk(body.config),revision})};else target={buzzer_switch:body.on!};
 const id=crypto.randomUUID();
 try{
  if(kind==='config'){
   const current=await latest(true);fresh(current);
   if(decodeRisk(current.readings.risk_config?.value)?.revision!==body.baseRevision)throw new ControlError('设备规则已更新，请重新载入后再编辑',409);
  }
  await db.prepare('INSERT INTO control_commands (id,kind,revision,desired,status,message,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?)').bind(id,kind,revision,JSON.stringify(target),'sending','正在等待设备回执',now,now).run();
  await setDeviceProperties(target);
  const status=kind==='config'?'acknowledged':'confirmed';
  const message=kind==='config'?'设备已确认并保存，等待属性回传；网页规则以设备回传为准':body.on?'设备已确认开启手动试鸣':'设备已确认关闭手动试鸣';
  await db.prepare('UPDATE control_commands SET status=?,message=?,updated_at=? WHERE id=?').bind(status,message,Date.now(),id).run();
  return {ok:true,id,status,message,revision};
 }catch(e){
  const message=e instanceof Error?e.message:'设备控制失败';
  await db.prepare('UPDATE control_commands SET status=?,message=?,updated_at=? WHERE id=?').bind('failed',message,Date.now(),id).run();
  throw e;
 }finally{await db.prepare('UPDATE control_locks SET busy_until=0 WHERE id=? AND revision=?').bind(kind,revision).run();}
}
