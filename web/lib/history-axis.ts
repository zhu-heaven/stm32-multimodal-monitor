export function historyAxis(start:number,end:number){
 const span=end-start;
 const steps=[60000,300000,600000,1800000,3600000,7200000,14400000,21600000,43200000,86400000,172800000,604800000];
 const step=steps.find(s=>span/s<=8)??604800000;
 const ticks:number[]=[];
 const offset=28800000;
 for(let t=Math.ceil((start+offset)/step)*step-offset;t<=end;t+=step)ticks.push(t);
 const format=(time:number)=>new Intl.DateTimeFormat('zh-CN',{timeZone:'Asia/Shanghai',...(span>=172800000?{month:'2-digit' as const,day:'2-digit' as const}:{}),...(span<172800000||step<86400000?{hour:'2-digit' as const,...(span<21600000?{minute:'2-digit' as const}:{}),hour12:false}:{})}).format(time);
 return {ticks,format};
}
