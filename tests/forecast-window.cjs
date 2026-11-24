const {build}=require('../web/node_modules/esbuild');
const assert=require('node:assert/strict');const path=require('node:path');
(async()=>{
 const out=await build({entryPoints:[path.join(__dirname,'../web/lib/forecast-data.ts')],bundle:true,write:false,platform:'node',format:'cjs'});
 const m={exports:{}};new Function('module','exports',out.outputFiles[0].text)(m,m.exports);
 for(const [date,days] of [['2026-11-23T18:00:00+08:00',['2026-11-22','2026-11-23','2026-11-24']],['2027-01-01T00:00:00+08:00',['2026-12-31','2027-01-01','2027-01-02']]]){
  const now=Date.parse(date),p=m.exports.forecastPeriod(now);assert.deepEqual(p.dates,days);
  const raw={timezone:'Asia/Shanghai',utc_offset_seconds:28800,daily_units:{precipitation_sum:'mm'},hourly_units:{precipitation:'mm'},daily:{time:days,precipitation_sum:[0.1,12.2,0]},hourly:{time:Array.from({length:72},(_,i)=>new Date(p.start+i*3600000+28800000).toISOString().slice(0,16)),precipitation:Array.from({length:72},(_,i)=>i===1?null:0)}};
  const f=m.exports.parseForecast(raw,now);assert.equal(f.total,12.3);assert.equal(f.average,4.1);assert.equal(f.hourly.length,72);assert.equal(f.hourly[1].amount,null);
  const missing=structuredClone(raw);missing.daily.precipitation_sum[1]=null;assert.throws(()=>m.exports.parseForecast(missing,now));
 }
 console.log('PASS Beijing date window, year boundary, sum/mean and missing-value handling');
})().catch(e=>{console.error(e);process.exitCode=1});
