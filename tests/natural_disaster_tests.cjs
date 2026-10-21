const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const base=path.join(__dirname,'../integrations/nodecg/broadcast');
const model=require(path.join(base,'graphics/natural-disaster-model.js'));
const elements=new Map(),reps=new Map();
let lastAnimation,cancellations=0;
function el(id){if(!elements.has(id))elements.set(id,{value:'',style:{},clientWidth:1570,scrollWidth:2000,
  classList:{toggle(){}},setAttribute(){},querySelectorAll:()=>[],animate(frames,options){lastAnimation={frames,options};return {cancel(){cancellations++;}};}});return elements.get(id);}
function rep(name,options={}){if(!reps.has(name)){let value=options.defaultValue;const listeners=[];reps.set(name,{get value(){return value;},set value(next){value=next;listeners.forEach(fn=>fn(next));},on(event,fn){listeners.push(fn);if(value!==undefined)fn(value);}});}return reps.get(name);}
const context={SWNaturalDisaster:model,nodecg:{Replicant:rep},NodeCG:{waitForReplicants:()=>Promise.resolve()},document:{getElementById:el,fonts:{ready:Promise.resolve()}},window:{},URLSearchParams,location:{search:''}};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(base,'graphics/broadcast-layout.js'),'utf8'),context);
context.SWBroadcastLayout=context.window.SWBroadcastLayout;
vm.runInContext(fs.readFileSync(path.join(base,'dashboard/natural-disaster.js'),'utf8'),context);
vm.runInContext(fs.readFileSync(path.join(base,'graphics/natural-disaster.js'),'utf8'),context);
(async()=>{
 await new Promise(resolve=>setImmediate(resolve));
 rep('lowerThird').value={visible:true,kind:'report'};rep('programHead').value={visible:true};rep('emergencyTicker').value={visible:false,bottom:42};
 const unaffected=JSON.stringify([rep('lowerThird').value,rep('programHead').value,rep('emergencyTicker').value]);
 await el('naturalTake').onclick();assert.equal(rep('naturalDisaster').value.visible,false,'empty TAKE must not air');
 el('naturalMessageInput').value='첫 문구\n둘째 문구';el('naturalSpeed').value='160';el('naturalLabel').value='자연재해';
 await el('naturalPreview').onclick();assert.equal(rep('naturalDisaster').value.visible,false,'preview is not program');
 await el('naturalTake').onclick();assert.equal(rep('naturalDisaster').value.visible,true);
 assert.equal(el('natural-text').textContent,'첫 문구　 •　 둘째 문구');
 assert.equal(lastAnimation.frames[0].transform,'translateX(1570px)');assert.equal(lastAnimation.frames[1].transform,'translateX(-2000px)');
 assert.equal(lastAnimation.options.duration,3570/160*1000);assert.equal(lastAnimation.options.iterations,Infinity);
 const layout=context.SWBroadcastLayout,active=rep('naturalDisaster').value;
 assert.equal(layout.contentBottom({visible:false},active,110),116);
 assert.equal(layout.naturalBottom({visible:true,bottom:42}),166);
 assert.equal(layout.contentBottom({visible:true,bottom:42},active,110),250,'caption is above both tickers');
 await el('naturalOut').onclick();assert.equal(rep('naturalDisaster').value.visible,false);assert.ok(cancellations>0,'OUT cancels scroll');
 assert.equal(JSON.stringify([rep('lowerThird').value,rep('programHead').value,rep('emergencyTicker').value]),unaffected,'other CG unchanged');
 assert.equal(rep('naturalDisasterDraft').value.message,'첫 문구\n둘째 문구','OUT preserves draft');
 assert.equal(model.normalize({speed:999}).speed,240);assert.equal(model.normalize({speed:-1}).speed,40);
 assert.equal(layout.contentBottom({visible:false},rep('naturalDisaster').value,110),110);
 console.log('PASS: empty guard, preview isolation, independent TAKE/OUT, repeat/direction/speed, stacking and restoration.');
})().catch(error=>{console.error(error);process.exitCode=1;});
