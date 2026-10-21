const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const elements=new Map();const get=id=>{if(!elements.has(id))elements.set(id,{style:{},classList:{add(){},remove(){},toggle(){}},textContent:''});return elements.get(id);};
let changed,rotate,rotationCount=0;
const context={document:{getElementById:get},window:{},location:{search:''},URLSearchParams,Intl,Date,
 SWBroadcastLayout:{weather:v=>v,watchObstacles(){},contentBottom:(e,n,b)=>b},
 nodecg:{Replicant:()=>({on:(event,fn)=>changed=fn})},setInterval(fn,ms){if(ms!==1000){rotate=fn;rotationCount++;}return rotationCount;},clearInterval(){},setTimeout:fn=>{fn();return 1;},clearTimeout(){}};
vm.runInNewContext(fs.readFileSync('integrations/nodecg/broadcast/graphics/weather.js','utf8'),context);
const value={visible:true,intervalSeconds:5,items:[{city:'광주',temp:'20℃',condition:'맑음'},{city:'목포',temp:'21℃',condition:'눈'}]};
changed(value);assert.ok(get('weatherIcon').src.endsWith('sunny.png'));rotate();assert.equal(get('weatherCity').textContent,'목포');assert.ok(get('weatherIcon').src.endsWith('snow.png'));
changed({...value,items:[value.items[0],{city:'목포',temp:'22℃',condition:'비'}]});
assert.equal(get('weatherCity').textContent,'목포');assert.equal(get('weatherTemp').textContent,'22');assert.ok(get('weatherIcon').src.endsWith('rain.png'));assert.equal(rotationCount,1,'refresh keeps city rotation timer running');rotate();assert.equal(get('weatherCity').textContent,'광주');
console.log('PASS: automatic data refresh preserves rotation and updates current temperature/icon.');
