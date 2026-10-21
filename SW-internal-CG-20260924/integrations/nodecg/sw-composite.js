"use strict";

(()=>{
  const preview=new URLSearchParams(location.search).get("preview")==="1",frames=new Map();
  const format=nodecg.Replicant(preview?"studioFormatDraft":"studioFormat",{defaultValue:SWStudio.normalize()});
  for(const layer of SWStudio.layers){const frame=document.createElement("iframe");frame.title=layer.name;frame.dataset.layer=layer.id;frame.src=SWStudio.graphicUrl(layer,preview);frames.set(layer.id,frame);document.body.append(frame);}
  format.on("change",value=>{const data=SWStudio.normalize(value);data.order.forEach((id,index)=>{const frame=frames.get(id);frame.style.zIndex=String(data.order.length-index);frame.style.visibility=preview&&!data.preview[id]?"hidden":"visible";});});
  // Every separated layer must be connected before the native renderer airs CG.
  window.swCgReady=()=>{try{return nodecg.socket.connected&&format.status==="declared"&&SWStudio.layers.every(layer=>{
    const child=frames.get(layer.id).contentWindow;return child.document.readyState==="complete"&&child.nodecg?.socket.connected&&child.nodecg.Replicant(preview?layer.draft:layer.rep).status==="declared";
  });}catch{return false;}};
})();
