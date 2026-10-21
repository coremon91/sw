"use strict";
(()=>{
  const input=document.getElementById("weatherApiKey"),save=document.getElementById("saveWeatherApiKey"),status=document.getElementById("weatherApiKeyStatus");
  let busy=false;
  function request(name,data){return Promise.race([nodecg.sendMessage(name,data),new Promise((_,reject)=>setTimeout(()=>reject(Error("API 설정 연결 대기 중 · NodeCG 서버 재시작 후 다시 시도하세요.")),6000))]);}
  function render(value){status.textContent=value.configured?"API 키 설정됨 · 저장된 키는 표시하지 않습니다.":"API 키 미설정";}
  save.onclick=async()=>{if(busy)return;if(!input.value.trim()){status.textContent="기상청 API 인증키를 입력하세요.";return;}busy=true;save.disabled=true;try{render(await request("weather:saveKey",{serviceKey:input.value}));input.value="";status.textContent="API 키 저장 완료 · 기상청 API 새로고침으로 조회하세요.";}catch(error){status.textContent=error.message||String(error);}finally{busy=false;save.disabled=false;}};
  request("weather:keyStatus").then(render).catch(error=>{status.textContent=error.message||String(error);});
})();
