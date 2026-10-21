"use strict";
(()=>{
  const $=id=>document.getElementById(id),model=SWStudio;
  const format=nodecg.Replicant("studioFormat",{defaultValue:model.normalize()}),draft=nodecg.Replicant("studioFormatDraft");
  const programReps=new Map(model.layers.map(layer=>[layer.id,nodecg.Replicant(layer.rep)]));
  const draftReps=new Map(model.layers.map(layer=>[layer.id,nodecg.Replicant(layer.draft)]));
  const prefixes={logo:"logo",weather:"weather",emergencyTicker:"emergency",election:"election"};
  const batchSelected=new Set();let batchBusy=false;
  const frames=new Map(),rows=new Map(),logoDraft=nodecg.Replicant("logoPreview");let selected="logo",preview=true,ready=false,current=model.normalize(),drag;
  const byId=id=>model.layers.find(layer=>layer.id===id);
  function notice(text){$("notice").textContent=text;}
  function syncRows(){
    if(rows.size!==model.layers.length)return;
    let on=0;const list=$("layerList");current.order.forEach((id,index)=>{const row=rows.get(id);if(list.children[index]!==row.root)list.insertBefore(row.root,list.children[index]||null);row.number.textContent=index+1;row.root.classList.toggle("selected",id===selected);row.select.setAttribute("aria-pressed",String(id===selected));row.check.checked=current.preview[id];row.batch.checked=batchSelected.has(id);row.root.classList.toggle("batch-selected",batchSelected.has(id));const live=Boolean(programReps.get(id).value?.visible);row.status.textContent=live?"ON · 현재 CG":"OFF";row.status.classList.toggle("live",live);if(live)on++;});
    $("liveCount").textContent=`${on} ON`;$("moveUp").disabled=!ready||current.order[0]===selected;$("moveDown").disabled=!ready||current.order.at(-1)===selected;
    $("formatName").textContent=current.name;
    const changed=JSON.stringify(current.order)!==JSON.stringify(model.normalize(format.value).order);$("applyOrder").disabled=!ready||!changed;$("orderStatus").textContent=changed?"순서 변경은 프리뷰에만 적용 중입니다.":"프리뷰 순서와 출력 순서가 같습니다.";
    updateLogoHandle();
  }
  function save(next){if(!ready)return;draft.value=model.normalize(next);}
  for(const layer of model.layers){
    const root=document.createElement("div");root.className="layer-row";
    const select=document.createElement("button");select.className="layer-select";select.setAttribute("aria-label",`${layer.name} 레이어 선택`);
    const number=document.createElement("span");number.className="layer-number";const text=document.createElement("span"),name=document.createElement("span"),status=document.createElement("span");name.className="layer-name";name.textContent=layer.name;status.className="on-state";text.append(name,status);select.append(number,text);select.onclick=()=>choose(layer.id);
    const check=document.createElement("input");check.type="checkbox";check.className="layer-check";check.setAttribute("aria-label",`${layer.name} PVW 표시`);check.onchange=()=>save({...current,preview:{...current.preview,[layer.id]:check.checked}});
    const batch=document.createElement("input");batch.type="checkbox";batch.className="batch-check";batch.setAttribute("aria-label",`${layer.name} 일괄 ON/OFF 선택`);batch.onchange=()=>{if(batch.checked)batchSelected.add(layer.id);else batchSelected.delete(layer.id);syncRows();updateButtons();};
    root.append(batch,select,check);rows.set(layer.id,{root,select,check,batch,number,status});programReps.get(layer.id).on("change",()=>syncRows());
  }
  function styleEditor(frame,layer){
    const doc=frame.contentDocument;if(!doc)return;
    const style=doc.createElement("style");style.textContent=`html,body{margin:0!important;background:#f6f8fb!important;color:#25374c!important;font-family:'Malgun Gothic',Arial,sans-serif!important}body{padding:14px!important}.preview-toolbar,#previewViewport,#viewport,#previewFrame,#preview,#dragSurface{display:none!important}body>strong{display:none!important}.actions{display:none!important}section{padding:0!important;border:0!important;background:transparent!important}input,select,textarea{max-width:100%;background:white!important}h2{font-size:15px!important}.grid,.pair{gap:10px!important}.hint{line-height:1.65}#message{color:#466987!important}.range-row{grid-template-columns:1fr 46px!important}`;doc.head.append(style);
    doc.querySelector(".studio-entry")?.remove();
    // Only the central composite renders previews; keep each editor's controls.
    doc.querySelectorAll("iframe#previewFrame,iframe#preview").forEach(previewFrame=>previewFrame.remove());
    if(layer.section!==undefined){const sections=Array.from(doc.querySelectorAll("body > section"));sections.forEach((section,index)=>{if(index!==layer.section)section.style.display="none";});}
  }
  function choose(id){
    selected=id;const layer=byId(id);$("selectedName").textContent=layer.name;$("selectedDetail").textContent=layer.detail;
    if(!frames.has(id)){const frame=document.createElement("iframe");frame.title=`${layer.name} 레이어 설정`;frame.src=`${layer.file}?standalone=true&studioEditor=1`;frame.onload=()=>{styleEditor(frame,layer);updateButtons();};frames.set(id,frame);$("editors").append(frame);}
    for(const [key,frame] of frames)frame.hidden=key!==id;syncRows();updateButtons();
  }
  function updateButtons(){const win=frames.get(selected)?.contentWindow;let connected=false;try{connected=Boolean(win?.nodecg?.socket.connected&&win.nodecg.Replicant(byId(selected).draft).status==="declared");}catch{}for(const id of ["selectedPreview","selectedTake","selectedOut"])$(id).disabled=!ready||!connected||batchBusy;$("batchTake").textContent=`체크한 ${batchSelected.size}개 ON`;$("batchTake").disabled=!ready||!nodecg.socket.connected||!batchSelected.size||batchBusy;$("batchOut").textContent=`체크한 ${batchSelected.size}개 OFF`;$("batchOut").disabled=$("batchTake").disabled;$("batchClear").disabled=batchBusy||!batchSelected.size;}
  async function action(type){
    const layer=byId(selected),frame=frames.get(selected);if(!frame||!ready)return;
    try{
      const win=frame.contentWindow;const target=type==="preview"?(layer.preview||"apply"):type==="take"?(layer.take||"take"):(layer.out||"out");
      const button=frame.contentDocument.getElementById(target);
      if(button){if(button.disabled)throw Error("설정 연결이 완료될 때까지 기다려 주세요.");button.click();}
      else if(type==="preview"&&selected==="logo")await win.pushPreview();else throw Error("설정창을 다시 불러와 주세요.");
      if(type!=="out")save({...current,preview:{...current.preview,[selected]:true}});
      notice(`${layer.name} ${type==="preview"?"미리보기 적용 요청":type==="take"?"TAKE 요청 · 레이어 ON 상태를 확인하세요.":"OUT 요청 · 레이어 OFF 상태를 확인하세요."}`);
    }catch(error){notice(error.message||String(error));}
  }
  $("batchClear").onclick=()=>{batchSelected.clear();syncRows();updateButtons();};
  $("batchTake").onclick=async()=>{
    if(!ready||batchBusy||!nodecg.socket.connected)return;
    batchBusy=true;updateButtons();
    const ids=current.order.filter(id=>batchSelected.has(id));
    try{
      const operations=ids.map(id=>{
        const layer=byId(id),prefix=prefixes[id];
        return {name:layer.name,collect(){
          const frame=frames.get(id);let value;
          if(frame){
            const win=frame.contentWindow,button=frame.contentDocument?.getElementById(layer.take||"take");
            if(!win?.nodecg?.socket.connected||win.nodecg.Replicant(layer.draft).status!=="declared"||!button||button.disabled)throw Error(`${layer.name}: 설정 로드가 완료될 때까지 기다려 주세요.`);
            const collector=id==="programHead"?win.collectHead:id==="naturalDisaster"?win.collectNaturalDisaster:win.collect;
            if(typeof collector!=="function")throw Error(`${layer.name}: 설정창을 새로고침해 주세요.`);
            value=collector();
          }else value=draftReps.get(id).value;
          if(!value)throw Error(`${layer.name}: 미리보기 설정이 없습니다.`);
          value=JSON.parse(JSON.stringify(value));
          if(id==="naturalDisaster"&&!SWNaturalDisaster.scrollText(value))throw Error("자연재해 스크롤 문구를 입력하세요.");
          if(id==="programHead"&&value.mode==="image"&&!value.image)throw Error("프로그램 헤드 파일을 선택하세요.");
          if(id==="logo"&&!value.image)throw Error("로고 이미지를 선택하세요.");
          return value;
        },async prepare(value){
          if(prefix)await nodecg.sendMessage(`${prefix}:update`,value);
          else draftReps.get(id).value=value;
        },async show(value){
          if(prefix)await nodecg.sendMessage(`${prefix}:show`);
          else programReps.get(id).value={...programReps.get(id).value,...value,visible:true};
        }};
      });
      const result=await SWStudioBatch.take(operations);
      save({...current,preview:{...current.preview,...Object.fromEntries(ids.map(id=>[id,true]))}});
      notice(`${result.on.join(" · ")} ON${result.failed.length?` / ON 확인 필요: ${result.failed.join(" · ")}`:" 완료"}`);
    }catch(error){notice(`일괄 ON 중단: ${error.message||error}`);}finally{batchBusy=false;syncRows();updateButtons();}
  };
  $("batchOut").onclick=async()=>{
    if(!ready||batchBusy||!nodecg.socket.connected)return;
    const ids=current.order.filter(id=>batchSelected.has(id));
    batchBusy=true;updateButtons();
    try{
      const result=await SWStudioBatch.out(ids.map(id=>({name:byId(id).name,async hide(){
        const prefix=prefixes[id];
        if(prefix)await nodecg.sendMessage(`${prefix}:hide`);
        else {const rep=programReps.get(id);rep.value={...rep.value,visible:false};}
      }})));
      const completed=result.off.length?`${result.off.join(" · ")} OFF 완료`:"";
      notice([completed,result.failed.length?`OFF 확인 필요: ${result.failed.join(" · ")}`:""].filter(Boolean).join(" / "));
    }catch(error){notice(`일괄 OFF 중단: ${error.message||error}`);}finally{batchBusy=false;syncRows();updateButtons();}
  };
  $("selectedPreview").onclick=()=>action("preview");$("selectedTake").onclick=()=>action("take");$("selectedOut").onclick=()=>action("out");
  $("moveUp").onclick=()=>save(model.move(current,selected,-1));$("moveDown").onclick=()=>save(model.move(current,selected,1));
  $("applyOrder").onclick=()=>{if(!ready)return;format.value={...model.normalize(format.value),order:[...current.order],name:current.name};notice("레이어 순서를 현재 CG에 적용했습니다. 각 레이어의 ON/OFF 상태는 유지됩니다.");};
  function mode(value){preview=value;$("previewMode").classList.toggle("active",value);$("programMode").classList.toggle("active",!value);$("previewMode").setAttribute("aria-pressed",String(value));$("programMode").setAttribute("aria-pressed",String(!value));$("previewTitle").textContent=value?"편집 프리뷰":"현재 CG 출력";$("composite").src=`/bundles/my-cg/graphics/sw-composite.html${value?"?preview=1":""}`;updateLogoHandle();}
  $("previewMode").onclick=()=>{if(!preview)mode(true);};$("programMode").onclick=()=>{if(preview)mode(false);};
  function updateLogoHandle(){
    const handle=$("logoDrag");handle.hidden=!preview||selected!=="logo"||!current.preview.logo||!logoDraft.value;if(handle.hidden)return;
    const data=SWBroadcastLayout.logo(logoDraft.value),scale=$("previewViewport").clientWidth/1920;
    handle.style.left=`${data.x*scale}px`;handle.style.top=`${data.y*scale}px`;handle.style.width=`${data.width*scale}px`;handle.style.height=`${data.height*scale}px`;
  }
  logoDraft.on("change",updateLogoHandle);
  $("logoDrag").onpointerdown=event=>{if(!ready)return;event.preventDefault();$("logoDrag").setPointerCapture(event.pointerId);drag={pointer:event.pointerId,x:event.clientX,y:event.clientY,value:SWBroadcastLayout.logo(logoDraft.value)};};
  $("logoDrag").onpointermove=event=>{if(!drag||event.pointerId!==drag.pointer)return;const scale=$("previewViewport").clientWidth/1920;logoDraft.value={...drag.value,x:Math.round(Math.max(0,Math.min(1900,1920-drag.value.width,drag.value.x+(event.clientX-drag.x)/scale))),y:Math.round(Math.max(0,Math.min(1060,1080-drag.value.height,drag.value.y+(event.clientY-drag.y)/scale)))};};
  $("logoDrag").onpointerup=$("logoDrag").onpointercancel=()=>{drag=null;};
  function resizeComposite(){$("composite").style.transform=`scale(${$("previewViewport").clientWidth/1920})`;$("composite").style.top="0";$("composite").style.left="0";updateLogoHandle();}
  new ResizeObserver(resizeComposite).observe($("previewViewport"));window.addEventListener("resize",resizeComposite);resizeComposite();
  draft.on("change",value=>{if(value){current=model.normalize(value);syncRows();}});format.on("change",syncRows);
  NodeCG.waitForReplicants(format,draft,...programReps.values(),...draftReps.values()).then(()=>{
    if(!draft.value)draft.value=model.normalize(format.value);
    if(!draftReps.get("programHead").value)draftReps.get("programHead").value=SWCaptionModel.head(programReps.get("programHead").value);
    if(!draftReps.get("lowerThird").value)draftReps.get("lowerThird").value={...SWCaptionModel.caption(programReps.get("lowerThird").value),keywordPlacement:"top"};
    if(!draftReps.get("naturalDisaster").value)draftReps.get("naturalDisaster").value=SWNaturalDisaster.normalize(programReps.get("naturalDisaster").value);
    current=model.normalize(draft.value);ready=true;choose(selected);syncRows();
  }).catch(error=>notice(`연결 실패: ${error.message||error}`));
  setInterval(()=>{const connected=Boolean(nodecg.socket.connected);$("connection").textContent=connected?"● NodeCG 연결됨":"● 연결 끊김";$("connection").classList.toggle("ok",connected);let healthy=false;try{healthy=Boolean($("composite").contentWindow.swCgReady?.());}catch{}$("previewState").textContent=healthy?(preview?"PVW · 편집용 합성":"현재 CG · SW CG ON 별도"):"CG 연결 / 로드 대기";updateButtons();},700);
})();
