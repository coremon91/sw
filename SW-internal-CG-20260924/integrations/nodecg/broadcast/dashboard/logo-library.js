"use strict";
(()=>{
  const select=document.getElementById("librarySelect"),info=document.getElementById("libraryInfo"),thumb=document.getElementById("libraryThumbnail"),size=document.getElementById("librarySize");
  let entries=[];
  function current(){return entries.find(entry=>entry.id===select.value);}
  function render(){const entry=current();thumb.hidden=!entry;size.disabled=!entry;if(entry){thumb.src=entry.thumbnail;info.textContent=`${entry.kind==="animation"?`움직이는 로고 · ${entry.frames} 프레임 · ${entry.fps} fps`:"정지 로고"} · ${entry.size.join(" × ")} 원본 · 화면 안에서 위치 유지${entry.note?` · ${entry.note}`:""}`; }else info.textContent="URL 직접 입력 가능 · 목록 선택은 프리뷰에만 반영됩니다.";}
  function sync(value){select.value=entries.find(entry=>entry.image===value?.image)?.id||"";render();}
  async function applySize(){const entry=current();if(!entry)return;for(const id of ["width","height"])document.getElementById(id).value=entry.defaults[id];document.getElementById("x").value=Math.max(0,Math.min(fieldNumber("x",1710),1920-entry.defaults.width));document.getElementById("y").value=Math.max(0,Math.min(fieldNumber("y",76),1080-entry.defaults.height));updateDragHandle();await pushPreview();}
  select.onchange=async()=>{const entry=current();render();if(!entry)return;clearTimeout(liveUpdateTimer);document.getElementById("image").value=entry.image;try{await applySize();}catch(error){info.textContent=error.message||String(error);}};
  size.onclick=()=>applySize().catch(error=>{info.textContent=error.message||String(error);});
  document.getElementById("logoReferencePosition").onclick=async()=>{
    clearTimeout(liveUpdateTimer);
    for(const [id,value] of Object.entries(SWBroadcastLayout.logoPreset))document.getElementById(id).value=value;
    updateDragHandle();
    try{await pushPreview();info.textContent="첨부 가이드의 우측 상단 영역에 맞췄습니다. 프리뷰 확인 후 TAKE하세요.";}
    catch(error){info.textContent=error.message||String(error);}
  };
  logo.on("change",sync);
  fetch("/bundles/my-cg/graphics/logo-library/catalog.json").then(response=>{if(!response.ok)throw Error("로고 목록을 불러오지 못했습니다.");return response.json();}).then(catalog=>{
    entries=catalog.entries;
    for(const kind of ["still","animation"]){const group=document.createElement("optgroup");group.label=kind==="still"?"정지 로고":"움직이는 로고";for(const entry of entries.filter(item=>item.kind===kind)){const option=document.createElement("option");option.value=entry.id;option.textContent=entry.label;group.append(option);}select.append(group);}
    for(const warning of catalog.warnings){const p=document.createElement("p");p.textContent=warning;document.getElementById("libraryWarnings").append(p);}
    sync(logo.value);
  }).catch(error=>{info.textContent=error.message||String(error);});
})();
