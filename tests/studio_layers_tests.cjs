const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const base=path.join(__dirname,'../integrations/nodecg');
const model=require(path.join(base,'broadcast/graphics/studio-model.js'));
const reps=new Map();
function rep(name,options={}){if(!reps.has(name)){let value=options.defaultValue;const listeners=[];reps.set(name,{status:'declared',get value(){return value;},set value(next){value=next;listeners.forEach(fn=>fn(next));},on(event,fn){listeners.push(fn);fn(value);}});}return reps.get(name);}
assert.equal(model.layers.length,7);
assert.deepEqual(model.normalize({order:['logo','logo','unknown']}).order,model.normalize().order);
assert.deepEqual(model.move({},'logo',-1).order,model.normalize().order);
assert.equal(model.move({},'logo',1).order[1],'logo');
function scene(preview){
 const frames=[];
 const context={SWStudio:model,URLSearchParams,location:{search:preview?'?preview=1':''},window:{},nodecg:{Replicant:rep,socket:{connected:true}},
 document:{body:{append(frame){frames.push(frame);}},createElement(){return {dataset:{},style:{},contentWindow:{document:{readyState:'complete'},nodecg:{socket:{connected:true},Replicant:rep}}};}}};
 vm.runInNewContext(fs.readFileSync(path.join(base,'sw-composite.js'),'utf8'),context);
 return {frames,ready:context.window.swCgReady};
}
const program=scene(false),preview=scene(true);
assert.equal(program.frames.length,7);assert.ok(program.ready());
assert.equal(program.frames.filter(frame=>frame.src.startsWith('lower-third.html')).length,3);
assert.ok(preview.frames.every(frame=>frame.src.includes('preview=1')));
const layer=(scene,id)=>scene.frames.find(frame=>frame.dataset.layer===id);
assert.equal(layer(preview,'emergencyTicker').style.visibility,'hidden');
const order=model.move(rep('studioFormatDraft').value,'logo',1);
order.preview.logo=false;rep('studioFormatDraft').value=order;
assert.equal(layer(preview,'logo').style.zIndex,'6');
assert.equal(layer(program,'logo').style.zIndex,'7','draft order must not alter program');
assert.equal(layer(preview,'logo').style.visibility,'hidden');
assert.equal(layer(program,'logo').style.visibility,'visible','preview mask must not hide program');
rep('studioFormat').value={...model.normalize(),order:order.order};
assert.equal(layer(program,'logo').style.zIndex,'6');
layer(program,'naturalDisaster').contentWindow.nodecg.socket.connected=false;
assert.equal(program.ready(),false,'one disconnected layer blocks stale output');assert.equal(preview.ready(),true);
for(const selected of ['programHead','lowerThird','naturalDisaster']){
 const elements=Object.fromEntries(['program-head','lower-third','natural-disaster'].map(id=>[id,{style:{}}]));
 vm.runInNewContext(fs.readFileSync(path.join(base,'broadcast/graphics/studio-layer-filter.js'),'utf8'),{URLSearchParams,location:{search:`?studioLayer=${selected}`},document:{getElementById:id=>elements[id]}});
 assert.equal(Object.values(elements).filter(el=>el.style.display!=='none').length,1,'one caption group element per layer');
}
const ctx={window:{},location:{search:'?studio=1'},URLSearchParams};vm.runInNewContext(fs.readFileSync(path.join(base,'broadcast/graphics/broadcast-layout.js'),'utf8'),ctx);
let obstacles;ctx.window.SWBroadcastLayout.watchObstacles({Replicant:rep},true,(e,n)=>{obstacles={e,n};});
rep('emergencyTickerPreview').value={bottom:54,visible:false};rep('naturalDisasterDraft').value={message:'시험',visible:false};
rep('studioFormatDraft').value={...order,preview:{...order.preview,emergencyTicker:true,naturalDisaster:true}};
assert.equal(obstacles.e.visible,true);assert.equal(obstacles.n.visible,true);
assert.equal(ctx.window.SWBroadcastLayout.contentBottom(obstacles.e,obstacles.n,110),262);
rep('studioFormatDraft').value={...order,preview:{...order.preview,emergencyTicker:false,naturalDisaster:false}};
assert.equal(ctx.window.SWBroadcastLayout.contentBottom(obstacles.e,obstacles.n,110),110);
console.log('PASS: seven independent layers, repaired order, isolated PVW mask/order, output apply, disconnect guard, obstacle preview.');
