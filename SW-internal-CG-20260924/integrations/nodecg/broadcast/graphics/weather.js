"use strict";
const scene=document.getElementById("scene"),card=document.getElementById("weatherCard");
const body=document.getElementById("weatherBody"),city=document.getElementById("weatherCity");
const temp=document.getElementById("weatherTemp"),icon=document.getElementById("weatherIcon"),clock=document.getElementById("weatherClock");
const previewMode=new URLSearchParams(location.search).get("preview")==="1";
let currentValue,emergencyValue,naturalValue,rotationTimer,transitionTimer,itemIndex=0,items=[],rotationKey="";
function number(value,fallback,min,max){const parsed=Number(value);return Number.isFinite(parsed)?Math.min(max,Math.max(min,parsed)):fallback;}
function iconFor(condition){const base="/bundles/my-cg/graphics/images/weather-v2/";return base+(condition==="맑음"?"sunny.png":["눈","비/눈"].includes(condition)?"snow.png":["비","소나기"].includes(condition)?"rain.png":"cloudy.png");}
function showItem(item,animate){
  clearTimeout(transitionTimer);
  const render=()=>{
    city.textContent=item?.city||"정보 없음";
    temp.textContent=String(item?.temp||"—").replace(/\s*(℃|°C)\s*$/i,"");
    icon.src=iconFor(item?.condition);icon.alt=item?.condition||"날씨 정보 없음";icon.style.visibility=item?"visible":"hidden";
    body.classList.remove("changing");
  };
  if(animate){body.classList.add("changing");transitionTimer=setTimeout(render,170);}else render();
}
function updatePosition(){card.style.bottom=`${SWBroadcastLayout.contentBottom(emergencyValue,naturalValue,number(currentValue?.bottom,110,0,800))}px`;}
nodecg.Replicant(previewMode?"weatherPreview":"weather").on("change",value=>{
  if(!value)return;
  currentValue=SWBroadcastLayout.weather(value);
  card.style.right=`${number(currentValue.right,112,0,1700)}px`;card.style.width=`${number(currentValue.width,100,90,180)}px`;
  city.style.fontSize=`${number(currentValue.fontSize,27,20,34)}px`;temp.style.fontSize=`${number(currentValue.fontSize,27,20,34)+5}px`;
  updatePosition();scene.classList.toggle("visible",previewMode||Boolean(value.visible));
  const selectedCity=items[itemIndex]?.city;
  items=Array.isArray(value.items)?value.items.filter(Boolean):[];
  itemIndex=Math.max(0,items.findIndex(item=>item.city===selectedCity));
  showItem(items[itemIndex],false);
  const interval=number(value.intervalSeconds,5,2,30)*1000;
  const nextKey=JSON.stringify([interval,items.map(item=>item.city)]);
  if(nextKey!==rotationKey){
    clearInterval(rotationTimer);rotationKey=nextKey;
    if(items.length>1)rotationTimer=setInterval(()=>{itemIndex=(itemIndex+1)%items.length;showItem(items[itemIndex],true);},interval);
  }
});
SWBroadcastLayout.watchObstacles(nodecg,previewMode,(emergency,natural)=>{emergencyValue=emergency;naturalValue=natural;updatePosition();});
function updateClock(){clock.textContent=new Intl.DateTimeFormat("en-GB",{timeZone:"Asia/Seoul",hour:"2-digit",minute:"2-digit",hourCycle:"h23"}).format(new Date());}
updateClock();setInterval(updateClock,1000);
