"use strict";
(()=>{
  const program=nodecg.Replicant("naturalDisaster",{defaultValue:SWNaturalDisaster.normalize()}),draft=nodecg.Replicant("naturalDisasterDraft");
  const panel=document.getElementById("naturalControl"),message=document.getElementById("naturalMessage"),status=document.getElementById("naturalStatus");
  let ready=false;
  function busy(value){for(const button of panel.querySelectorAll("button"))button.disabled=value||!ready;}
  function collect(){return SWNaturalDisaster.normalize({label:document.getElementById("naturalLabel").value,message:document.getElementById("naturalMessageInput").value,speed:document.getElementById("naturalSpeed").value});}
  window.collectNaturalDisaster=collect;
  async function run(action){if(!ready)return;busy(true);try{await action();message.textContent="적용 완료";}catch(error){message.textContent=error.message||String(error);}finally{busy(false);}}
  function preview(){const value=collect();if(!SWNaturalDisaster.scrollText(value))throw Error("스크롤 문구를 입력하세요.");draft.value=value;return value;}
  document.getElementById("naturalPreview").onclick=()=>run(preview);
  document.getElementById("naturalTake").onclick=()=>run(()=>{program.value={...preview(),visible:true};});
  document.getElementById("naturalOut").onclick=()=>run(()=>{program.value={...SWNaturalDisaster.normalize(program.value),visible:false};});
  program.on("change",value=>{const on=Boolean(value?.visible);status.textContent=on?"자연재해 스크롤 ON":"자연재해 스크롤 OFF";status.classList.toggle("live",on);});
  NodeCG.waitForReplicants(program,draft).then(()=>{
    if(!draft.value)draft.value=SWNaturalDisaster.normalize(program.value);
    const value=SWNaturalDisaster.normalize(draft.value);
    document.getElementById("naturalLabel").value=value.label;document.getElementById("naturalMessageInput").value=value.message;document.getElementById("naturalSpeed").value=value.speed;
    ready=true;busy(false);
  }).catch(error=>{message.textContent=`연결 실패: ${error.message||error}`;});
  busy(false);
})();
