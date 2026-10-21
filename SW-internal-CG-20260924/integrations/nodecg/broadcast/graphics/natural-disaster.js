"use strict";
(()=>{
  const root=document.getElementById("natural-disaster"),text=document.getElementById("natural-text"),track=document.getElementById("natural-window");
  const preview=new URLSearchParams(location.search).get("preview")==="1";
  const program=nodecg.Replicant("naturalDisaster",{defaultValue:SWNaturalDisaster.normalize()});
  const rep=preview?nodecg.Replicant("naturalDisasterDraft"):program;
  let value=SWNaturalDisaster.normalize(),animation;
  function render(){
    animation?.cancel();animation=null;
    const content=SWNaturalDisaster.scrollText(value),visible=Boolean(content)&&(preview||value.visible);
    document.getElementById("natural-label").textContent=value.label;text.textContent=content;
    root.classList.toggle("visible",visible);root.setAttribute("aria-hidden",String(!visible));
    if(!visible||root.style.display==="none")return;
    const width=track.clientWidth,length=text.scrollWidth;
    animation=text.animate([{transform:`translateX(${width}px)`},{transform:`translateX(${-length}px)`}],
      {duration:(width+length)/value.speed*1000,iterations:Infinity,easing:"linear"});
  }
  rep.on("change",next=>{value=SWNaturalDisaster.normalize(next);render();});
  SWBroadcastLayout.watchObstacles(nodecg,preview,(emergency)=>{root.style.bottom=`${SWBroadcastLayout.naturalBottom(emergency)}px`;});
  document.fonts.ready.then(render);
})();
