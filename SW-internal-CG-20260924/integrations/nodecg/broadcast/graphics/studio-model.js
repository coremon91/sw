"use strict";
(function(root){
  const layers=[
    {id:"logo",name:"상단 로고",detail:"이미지 · 위치 · 크기 · 불투명도",file:"logo.html",rep:"logo",draft:"logoPreview"},
    {id:"weather",name:"날씨 CG",detail:"API 설정 · 지역 · 기온 · 시계",file:"weather.html",rep:"weather",draft:"weatherPreview"},
    {id:"programHead",name:"프로그램 헤드",detail:"이미지 파일 · 프로그램명",file:"lower-third.html",rep:"programHead",draft:"programHeadDraft",section:0,preview:"headPreview",take:"headTake",out:"headOut"},
    {id:"lowerThird",name:"자막",detail:"키워드 · 리포트 · 인터뷰 동시 선택",file:"lower-third.html",rep:"lowerThird",draft:"lowerThirdDraft",section:1,preview:"captionPreview",take:"captionTake",out:"captionOut"},
    {id:"naturalDisaster",name:"자연재해 스크롤",detail:"자연재해 안내 · 반복 스크롤",file:"lower-third.html",rep:"naturalDisaster",draft:"naturalDisasterDraft",section:2,preview:"naturalPreview",take:"naturalTake",out:"naturalOut"},
    {id:"emergencyTicker",name:"재난 경보",detail:"기존 재난 CG",file:"emergency.html",rep:"emergencyTicker",draft:"emergencyTickerPreview"},
    {id:"election",name:"선거 CG",detail:"개표 정보 · 후보 · 사진",file:"election.html",rep:"election",draft:"electionPreview"}
  ];
  const defaults=layers.map(layer=>layer.id);
  function normalize(value={}){
    value=value||{};
    const order=[...new Set([...(Array.isArray(value.order)?value.order:[]),...defaults])].filter(id=>defaults.includes(id));
    const preview={};for(const id of defaults)preview[id]=typeof value.preview?.[id]==="boolean"?value.preview[id]:["logo","weather","programHead","lowerThird"].includes(id);
    return {version:1,name:String(value.name||"KBC 통합 포맷").slice(0,80),order,preview};
  }
  function move(value,id,delta){const result=normalize(value),index=result.order.indexOf(id),next=index+Math.sign(delta);if(index>=0&&next>=0&&next<result.order.length)[result.order[index],result.order[next]]=[result.order[next],result.order[index]];return result;}
  function graphicUrl(layer,preview){return `${layer.file}?studio=1&studioLayer=${layer.id}${preview?"&preview=1":""}`;}
  const api={layers,normalize,move,graphicUrl};
  if(typeof module!=="undefined"&&module.exports)module.exports=api;else root.SWStudio=api;
})(typeof window!=="undefined"?window:globalThis);
