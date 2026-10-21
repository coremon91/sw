"use strict";
const root=document.getElementById("lower-third"),headRoot=document.getElementById("program-head"),headImage=document.getElementById("head-image");
const previewMode=new URLSearchParams(location.search).get("preview")==="1";
const program=nodecg.Replicant("lowerThird"),headProgram=nodecg.Replicant("programHead",{defaultValue:SWCaptionModel.head()});
const captionRep=previewMode?nodecg.Replicant("lowerThirdDraft"):program;
const headRep=previewMode?nodecg.Replicant("programHeadDraft"):headProgram;
let current=SWCaptionModel.caption(),currentHead=SWCaptionModel.head(),emergencyPosition,naturalPosition;
function fit(element,start,min,maxHeight){let size=start;element.style.fontSize=`${size}px`;while(size>min&&(element.scrollWidth>element.clientWidth||(maxHeight&&element.scrollHeight>maxHeight))){size-=2;element.style.fontSize=`${size}px`;}}
function fitText(){
  fit(document.getElementById("keyword-headline"),current.keywordPlacement==="top"?36:62,28,0);
  fit(document.getElementById("report-headline"),54,38,124);
  fit(document.getElementById("quote"),46,32,118);
  if(currentHead.mode!=="image"){fit(document.getElementById("head-line1"),26,14,0);fit(document.getElementById("head-line2"),43,16,0);}
}
function updateLowerPosition(){
  const base=SWBroadcastLayout.contentBottom(emergencyPosition,naturalPosition,110),enabled=current.enabledKinds;
  const reportBottom=base; // Report and interview share the same lower-third position.
  document.getElementById("interview-caption").style.bottom=`${base}px`;
  document.getElementById("report-caption").style.bottom=`${reportBottom}px`;
  document.getElementById("keyword-caption").style.bottom=`${enabled.includes("interview")?base+210:enabled.includes("report")?base+152:base}px`;
  headRoot.style.bottom=`${SWBroadcastLayout.contentBottom(emergencyPosition,naturalPosition,currentHead.mode==="image"?currentHead.bottom:110)}px`;
}
captionRep.on("change",value=>{
  if(!value)return;current=SWCaptionModel.caption(value);
  for(const kind of ["keyword","report","interview"])document.getElementById(`${kind}-caption`).hidden=!current.enabledKinds.includes(kind);
  document.getElementById("keyword-caption").className=`caption-bar keyword-${current.keywordPlacement}`;
  document.getElementById("keyword-headline").textContent=current.keywordText;
  document.getElementById("report-headline").textContent=current.reportText;
  document.getElementById("speaker-name").textContent=current.speakerName;document.getElementById("speaker-role").textContent=current.speakerRole;document.getElementById("quote").textContent=current.quote;
  const visible=(previewMode||current.visible)&&current.enabledKinds.length>0;root.classList.toggle("visible",visible);root.setAttribute("aria-hidden",String(!visible));updateLowerPosition();fitText();
});
headRep.on("change",value=>{
  if(!value)return;currentHead=SWCaptionModel.head(value);const image=currentHead.mode==="image";
  document.getElementById("head-line1").textContent=currentHead.line1;document.getElementById("head-line2").textContent=currentHead.line2;
  headRoot.classList.toggle("image-head",image);headImage.hidden=!image;
  if(image&&headImage.getAttribute("src")!==currentHead.image)headImage.src=currentHead.image;
  headRoot.style.left=`${image?currentHead.x:104}px`;headRoot.style.width=`${image?currentHead.width:144}px`;headRoot.style.height=`${image?currentHead.height:140}px`;
  headImage.style.opacity=String(currentHead.opacity/100);
  const visible=(previewMode||currentHead.visible)&&(!image||Boolean(currentHead.image));headRoot.classList.toggle("visible",visible);headRoot.setAttribute("aria-hidden",String(!visible));updateLowerPosition();fitText();
});
document.fonts.ready.then(fitText);
SWBroadcastLayout.watchObstacles(nodecg,previewMode,(emergency,natural)=>{emergencyPosition=emergency;naturalPosition=natural;updateLowerPosition();});
