"use strict";
(function(root){
  function normalize(value={}) {
    value=value||{};const speed=Number(value.speed);
    return {label:String(value.label??"자연재해").slice(0,12),message:String(value.message??"").slice(0,4000),
      speed:Number.isFinite(speed)?Math.min(240,Math.max(40,speed)):110,visible:Boolean(value.visible)};
  }
  const api={normalize,scrollText:value=>normalize(value).message.split(/\r?\n/).map(line=>line.trim()).filter(Boolean).join("　 •　 ")};
  if(typeof module!=="undefined"&&module.exports)module.exports=api;else root.SWNaturalDisaster=api;
})(typeof window!=="undefined"?window:globalThis);
