"use strict";
(function(root) {
  const version = "split-captions-v1";
  const text = (value, limit) => String(value ?? "").slice(0, limit);
  function caption(value = {}) {
    value = value || {};
    if (value.captionLayout !== version) {
      return {captionLayout:version, kind:"keyword", enabledKinds:["keyword"], keywordPlacement:"bottom", keywordText:[value.name,value.title].filter(Boolean).join(" · "), reportText:"", speakerName:"", speakerRole:"", quote:"", visible:Boolean(value.visible)};
    }
    return {captionLayout:version, kind:["keyword","report","interview"].includes(value.kind)?value.kind:"keyword",
      enabledKinds:Array.isArray(value.enabledKinds)?[...new Set(value.enabledKinds)].filter(kind=>["keyword","report","interview"].includes(kind)):[ ["keyword","report","interview"].includes(value.kind)?value.kind:"keyword" ],
      keywordPlacement:value.keywordPlacement==="bottom"?"bottom":"top", keywordText:text(value.keywordText,100), reportText:text(value.reportText,160),
      speakerName:text(value.speakerName,30),speakerRole:text(value.speakerRole,60),quote:text(value.quote,220),visible:Boolean(value.visible)};
  }
  function head(value = {}) {
    value=value||{};const number=(key,fallback,min,max)=>{const raw=value[key],n=Number(raw);return raw==null||raw===""||!Number.isFinite(n)?fallback:Math.max(min,Math.min(max,n));};
    const image=String(value.image||"");
    return {line1:text(value.line1??"MORNING",14),line2:text(value.line2??"WIDE",14),visible:Boolean(value.visible),mode:value.mode==="image"?"image":"text",
      image:/^(?:https?:\/\/|\/(?!\/))/.test(image)?image:"",x:number("x",104,0,1900),bottom:number("bottom",110,0,1060),width:number("width",144,20,800),height:number("height",140,20,800),opacity:number("opacity",100,0,100)};
  }
  const api = {version,caption,head};
  if(typeof module!=="undefined"&&module.exports)module.exports=api;else root.SWCaptionModel=api;
})(typeof window!=="undefined"?window:globalThis);
