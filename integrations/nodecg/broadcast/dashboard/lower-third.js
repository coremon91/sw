"use strict";
const fieldIds=["keywordText","reportText","speakerName","speakerRole","quote"];
const program=nodecg.Replicant("lowerThird"),headProgram=nodecg.Replicant("programHead",{defaultValue:SWCaptionModel.head()});
const draft=nodecg.Replicant("lowerThirdDraft"),headDraft=nodecg.Replicant("programHeadDraft");
const message=document.getElementById("message");
const names={keyword:"키워드",report:"리포트",interview:"인터뷰"};
let ready=false,applying=false;
const kindChecks={keyword:"enableKeyword",report:"enableReport",interview:"enableInterview"};
function collect(){const value={captionLayout:SWCaptionModel.version,keywordPlacement:"top",visible:false};for(const id of fieldIds)value[id]=document.getElementById(id).value;value.enabledKinds=Object.keys(kindChecks).filter(kind=>document.getElementById(kindChecks[kind]).checked);value.kind=value.enabledKinds[0]||"keyword";return SWCaptionModel.caption(value);}
function collectHead(){return SWCaptionModel.head({line1:document.getElementById("head1").value,line2:document.getElementById("head2").value,mode:document.getElementById("headMode").value,image:document.getElementById("headImage").value,x:document.getElementById("headX").value,bottom:document.getElementById("headBottom").value,width:document.getElementById("headWidth").value,height:document.getElementById("headHeight").value,opacity:document.getElementById("headOpacity").value});}
function readDraft(value){for(const id of fieldIds)document.getElementById(id).value=value[id]??"";for(const kind of Object.keys(kindChecks))document.getElementById(kindChecks[kind]).checked=value.enabledKinds.includes(kind);}
function busy(value){applying=value;for(const button of document.querySelectorAll("#headPreview,#headTake,#headOut,#captionPreview,#captionTake,#captionOut"))button.disabled=value||!ready;}
async function run(action){if(!ready||applying)return;busy(true);try{await action();message.textContent="적용 완료";}catch(error){message.textContent=error.message||String(error);}finally{busy(false);}}
function captionPreview(){draft.value=collect();}
function headPreview(){const value=collectHead();if(value.mode==="image"&&!value.image)throw Error("프로그램 헤드 파일을 선택하세요.");headDraft.value=value;}
// Each bus is a separate Replicant. Editing and OUT never alter the other bus.
document.getElementById("headPreview").onclick=()=>run(headPreview);
document.getElementById("headTake").onclick=()=>run(()=>{headPreview();headProgram.value={...collectHead(),visible:true};});
document.getElementById("headOut").onclick=()=>run(()=>{headProgram.value={...SWCaptionModel.head(headProgram.value),visible:false};});
document.getElementById("captionPreview").onclick=()=>run(captionPreview);
document.getElementById("captionTake").onclick=()=>run(()=>{captionPreview();program.value={...program.value,...collect(),visible:true};});
document.getElementById("captionOut").onclick=()=>run(()=>{program.value={...program.value,visible:false};});
function headSections(){const image=document.getElementById("headMode").value==="image";document.getElementById("headImageFields").hidden=false;document.getElementById("headTextFields").hidden=image;}
document.getElementById("headMode").onchange=headSections;
program.on("change",value=>{if(!value)return;const data=SWCaptionModel.caption(value);const status=document.getElementById("captionStatus");status.textContent=data.visible?`자막 ON · ${data.enabledKinds.map(kind=>names[kind]).join(" + ")||"선택 없음"}`:"자막 OFF";status.classList.toggle("live",data.visible);});
headProgram.on("change",value=>{if(!value)return;const status=document.getElementById("headStatus");status.textContent=value.visible?"프로그램 헤드 ON":"프로그램 헤드 OFF";status.classList.toggle("live",Boolean(value.visible));});
NodeCG.waitForReplicants(program,headProgram,draft,headDraft).then(()=>{
  if(!draft.value||draft.value.keywordPlacement!=="top")draft.value={...SWCaptionModel.caption(draft.value||program.value),keywordPlacement:"top"};
  if(!headDraft.value)headDraft.value=SWCaptionModel.head(headProgram.value);
  readDraft(SWCaptionModel.caption(draft.value));const head=SWCaptionModel.head(headDraft.value);document.getElementById("head1").value=head.line1;document.getElementById("head2").value=head.line2;
  for(const [field,key] of Object.entries({headMode:"mode",headImage:"image",headX:"x",headBottom:"bottom",headWidth:"width",headHeight:"height",headOpacity:"opacity"}))document.getElementById(field).value=head[key];headSections();
  ready=true;busy(false);
}).catch(error=>{message.textContent=`연결 실패: ${error.message||error}`;});
busy(false);
const viewport=document.getElementById("viewport"),preview=document.getElementById("preview");new ResizeObserver(()=>{preview.style.transform=`scale(${viewport.clientWidth/1920})`;}).observe(viewport);
