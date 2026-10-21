// Regression: independent head/caption buses and draft isolation from program.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const base = path.join(__dirname, '../integrations/nodecg/broadcast');
const model = require(path.join(base, 'graphics/caption-layout.js'));
const elements = new Map(), reps = new Map();
function element(id) {
  if (!elements.has(id)) elements.set(id, {value:'', textContent:'', hidden:false,
    style:{}, clientWidth:600, classList:{toggle(){}}});
  return elements.get(id);
}
function rep(name, options = {}) {
  if (!reps.has(name)) {
    let value = options.defaultValue;
    const listeners = [];
    reps.set(name, {get value(){return value;}, set value(next){value=next;listeners.forEach(fn=>fn(next));},
      on(event, fn){listeners.push(fn);if(value!==undefined)fn(value);}});
  }
  return reps.get(name);
}
rep('lowerThird').value = {name:'기존 제목', title:'보조 문구', visible:true};
const original = JSON.stringify(rep('lowerThird').value);
vm.runInNewContext(fs.readFileSync(path.join(base, 'dashboard/lower-third.js'), 'utf8'), {
  SWCaptionModel:model, nodecg:{Replicant:rep}, NodeCG:{waitForReplicants:()=>Promise.resolve()},
  document:{getElementById:element, querySelectorAll:()=>[]}, ResizeObserver:class {observe(){}},
});
(async()=>{
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(JSON.stringify(rep('lowerThird').value), original, 'opening controls must not rewrite program');
  assert.equal(element('keywordText').value, '기존 제목 · 보조 문구');
  assert.equal(rep('lowerThirdDraft').value.keywordPlacement,'top','legacy draft opens at confirmed keyword position');
  element('head1').value='MORNING';element('head2').value='WIDE';
  await element('headTake').onclick();
  assert.equal(rep('programHead').value.visible,true);
  assert.equal(JSON.stringify(rep('lowerThird').value), original, 'head TAKE preserves caption');
  element('keywordText').value='새 키워드';
  await element('captionPreview').onclick();
  assert.equal(rep('lowerThirdDraft').value.keywordPlacement,'top');
  assert.equal(JSON.stringify(rep('lowerThird').value),original,'preview preserves program');
  await element('captionTake').onclick();
  assert.equal(rep('lowerThird').value.keywordText,'새 키워드');
  await element('headOut').onclick();
  assert.equal(rep('programHead').value.visible,false);
  assert.equal(rep('lowerThird').value.visible,true,'head OUT preserves caption ON');
  element('enableKeyword').checked=false;element('enableReport').checked=true;element('reportText').value='리포트 내용';
  await element('captionPreview').onclick();
  assert.equal(rep('lowerThird').value.kind,'keyword');
  await element('captionTake').onclick();
  assert.equal(rep('lowerThird').value.kind,'report');
  assert.equal(rep('programHead').value.visible,false,'caption TAKE preserves head OFF');
  element('enableReport').checked=false;element('enableInterview').checked=true;element('speakerName').value='홍길동';element('speakerRole').value='시민';element('quote').value='첫째 줄\n둘째 줄';
  await element('captionTake').onclick();
  assert.equal(rep('lowerThird').value.quote,'첫째 줄\n둘째 줄');
  assert.equal(rep('lowerThird').value.speakerName,'홍길동');
  await element('headTake').onclick();await element('captionOut').onclick();
  assert.equal(rep('programHead').value.visible,true,'caption OUT preserves head ON');
  assert.equal(rep('lowerThird').value.visible,false);
  assert.equal(rep('lowerThirdDraft').value.quote,'첫째 줄\n둘째 줄','OUT preserves edited draft');
  element('enableKeyword').checked=true;element('enableReport').checked=true;
  await element('captionPreview').onclick();
  assert.deepEqual(Array.from(rep('lowerThirdDraft').value.enabledKinds),['keyword','report','interview']);
  assert.equal(rep('lowerThird').value.visible,false,'checkbox editing preserves program OUT');
  await element('captionTake').onclick();
  assert.deepEqual(Array.from(rep('lowerThird').value.enabledKinds),['keyword','report','interview']);
  element('headMode').value='image';element('headImage').value='/assets/my-cg/program-head/test.png';element('headWidth').value='240';element('headHeight').value='120';
  await element('headPreview').onclick();assert.equal(rep('programHead').value.mode,'text');
  await element('headTake').onclick();assert.equal(rep('programHead').value.mode,'image');assert.equal(rep('programHead').value.width,240);
  assert.equal(rep('lowerThird').value.visible,true);
  assert.equal(model.head({image:'javascript:alert(1)'}).image,'');
  assert.equal(model.caption(null).visible,false);
  console.log('PASS: legacy load, three caption styles, preview isolation, independent TAKE/OUT.');
})().catch(error=>{console.error(error);process.exitCode=1;});
