"use strict";
(()=>{
  const select=document.getElementById("headFileSelect"),upload=document.getElementById("headFileUpload"),message=document.getElementById("headFileMessage"),files=nodecg.Replicant("programHeadFiles",{defaultValue:[]});
  const defaultImage="/bundles/my-cg/graphics/program-head-default.svg";
  function options(){const value=document.getElementById("headImage").value;select.replaceChildren();for(const item of [{name:"파일을 선택하세요",url:""},{name:"MORNING WIDE 기본 헤드",url:defaultImage},...(files.value||[])]){const option=document.createElement("option");option.value=item.url;option.textContent=item.name;select.append(option);}select.value=value;}
  function choose(url,width,height){document.getElementById("headImage").value=url;document.getElementById("headMode").value="image";headSections();if(width&&height){const scale=Math.min(1,144/width,200/height);document.getElementById("headWidth").value=Math.max(20,Math.round(width*scale));document.getElementById("headHeight").value=Math.max(20,Math.round(height*scale));}headPreview();}
  files.on("change",options);
  NodeCG.waitForReplicants(files,headDraft,headProgram).then(()=>setTimeout(options,0));
  select.onchange=()=>{if(!select.value)return;const item=(files.value||[]).find(item=>item.url===select.value);try{choose(select.value,item?.width||144,item?.height||140);message.textContent="선택한 파일을 프리뷰에 적용했습니다.";}catch(error){message.textContent=error.message;}};
  upload.onchange=async()=>{
    const file=upload.files[0];if(!file)return;
    const extension=file.name.split(".").pop().toLowerCase();
    if(!["png","jpg","jpeg","webp","gif","svg"].includes(extension)||file.size>20*1024*1024){message.textContent="지원되는 이미지 파일(최대 20 MB)을 선택하세요.";upload.value="";return;}
    upload.disabled=true;busy(true);const objectUrl=URL.createObjectURL(file);
    try{
      const image=new Image();image.src=objectUrl;await image.decode();
      if(!image.naturalWidth||!image.naturalHeight)throw Error("이미지를 읽을 수 없습니다.");
      const name=`head-${crypto.randomUUID()}.${extension}`,data=new FormData();data.append("file",file,name);
      const response=await fetch("/assets/my-cg/program-head",{method:"POST",body:data,credentials:"same-origin"});if(!response.ok)throw Error(`파일 저장 실패 (${response.status})`);
      const item={name:file.name,url:`/assets/my-cg/program-head/${name}`,width:image.naturalWidth,height:image.naturalHeight};
      files.value=[...(files.value||[]),item];choose(item.url,item.width,item.height);options();message.textContent=`${file.name} 저장 · 프리뷰 적용 완료`;
    }catch(error){message.textContent=error.message||String(error);}finally{URL.revokeObjectURL(objectUrl);upload.disabled=false;upload.value="";busy(false);}
  };
})();
