"use strict";
// Preserve custom positions after the operator saves the new layout version.
window.SWBroadcastLayout = (() => {
  const version = "morning-v1";
  const logoPreset = {x:1601,y:72,width:223,height:48};
  const weatherPreset = {right:112,bottom:110,width:100,fontSize:27};
  function normalize(value,preset) {return {...value,...(value.layoutVersion===version?{}:preset),layoutVersion:version};}
  return {
    version,logoPreset,weatherPreset,
    logo(value) {
      const result=normalize(value,logoPreset);
      if(!result.image||result.image==="/bundles/my-cg/graphics/images/logo.png")result.image="/bundles/my-cg/graphics/kbc-company.svg";
      return result;
    },
    weather:value=>normalize(value,weatherPreset),
    watchObstacles(api,preview,callback) {
      const studio=preview&&new URLSearchParams(location.search).get("studio")==="1";
      let emergency,natural,format;
      const update=()=>{const enabled=format?.preview||{};callback(studio?{...emergency,visible:Boolean(enabled.emergencyTicker)}:emergency,studio?{...natural,visible:Boolean(enabled.naturalDisaster)}:natural);};
      api.Replicant(studio?"emergencyTickerPreview":"emergencyTicker").on("change",value=>{emergency=value;update();});
      api.Replicant(preview?"naturalDisasterDraft":"naturalDisaster").on("change",value=>{natural=preview&&!studio&&value?{...value,visible:true}:value;update();});
      if(studio)api.Replicant("studioFormatDraft").on("change",value=>{format=value;update();});
    },
    naturalBottom(emergency) {return this.emergencyBottom(emergency,32);},
    contentBottom(emergency,natural,base) {
      return natural?.visible&&String(natural.message||"").trim()?Math.max(this.emergencyBottom(emergency,base),this.naturalBottom(emergency)+72+12):this.emergencyBottom(emergency,base);
    },
    emergencyBottom(value,base) {
      if(!value?.visible)return base;
      const bottom=Number(value.bottom);
      return Math.max(base,(Number.isFinite(bottom)?bottom:54)+104+20);
    }
  };
})();
