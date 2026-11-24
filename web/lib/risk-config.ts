export const riskMetrics=[
 {name:'平均位移相对变化',unit:'mm',scale:100,max:75},
 {name:'土壤相对湿度',unit:'%',scale:10,max:100},
 {name:'雨滴板湿润度',unit:'%',scale:10,max:100},
 {name:'合成倾角相对变化',unit:'°',scale:100,max:180},
 {name:'三日累计降水（昨今明）',unit:'mm',scale:10,max:6000}
];
export type RiskConfig={revision:number;weights:number[];thresholds:number[][]};
export const defaultRisk:RiskConfig={revision:0,weights:[32,28,20,12,8],thresholds:[[.5,2,5],[30,60,80],[20,50,80],[.2,.5,1.5],[20,50,150]]};
export function validateRisk(input:unknown):RiskConfig{
 const c=input as RiskConfig;
 if(!c||!Number.isInteger(c.revision)||c.revision<0||c.revision>2147483647||!Array.isArray(c.weights)||c.weights.length!==5||!c.weights.every(w=>Number.isInteger(w)&&w>=0&&w<=100)||c.weights.reduce((a,b)=>a+b,0)!==100)throw Error('五项权重需为0～100的整数，合计100%');
 if(!Array.isArray(c.thresholds)||c.thresholds.length!==5)throw Error('需要五项指标的分档边界');
 for(let i=0;i<5;i++){const t=c.thresholds[i],m=riskMetrics[i];if(!Array.isArray(t)||t.length!==3||!t.every(x=>typeof x==='number'&&Number.isFinite(x)&&x>0&&x<=m.max&&Math.abs(x*m.scale-Math.round(x*m.scale))<.000001)||!(t[0]<t[1]&&t[1]<t[2]))throw Error(`${m.name}的三条边界需递增，范围0～${m.max}${m.unit}`);}
 return {revision:c.revision,weights:[...c.weights],thresholds:c.thresholds.map(t=>[...t])};
}
export function encodeRisk(c:RiskConfig){validateRisk(c);return [1,c.revision,...c.weights,...c.thresholds.flatMap((t,i)=>t.map(n=>Math.round(n*riskMetrics[i].scale)))].join(',');}
export function decodeRisk(value:unknown):RiskConfig|null{
 if(typeof value!=='string'||!/^\d+(,\d+){21}$/.test(value))return null;const n=value.split(',').map(Number);if(n[0]!==1)return null;
 try{return validateRisk({revision:n[1],weights:n.slice(2,7),thresholds:riskMetrics.map((m,i)=>n.slice(7+i*3,10+i*3).map(v=>v/m.scale))});}catch{return null;}
}
