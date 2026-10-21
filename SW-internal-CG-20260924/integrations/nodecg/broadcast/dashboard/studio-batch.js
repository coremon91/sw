"use strict";
// Collect and validate every layer before any output is changed. Preparation may
// update previews; a preparation failure must never start a partial TAKE.
(function(root){
  async function take(operations){
    if(!operations.length)throw Error("ON할 레이어를 체크하세요.");
    const prepared=operations.map(operation=>({operation,value:operation.collect()}));
    await Promise.all(prepared.map(({operation,value})=>operation.prepare(value)));
    const results=await Promise.allSettled(prepared.map(({operation,value})=>Promise.resolve().then(()=>operation.show(value))));
    return {on:prepared.filter((_,i)=>results[i].status==="fulfilled").map(({operation})=>operation.name),failed:prepared.filter((_,i)=>results[i].status==="rejected").map(({operation})=>operation.name)};
  }
  // OUT deliberately skips draft collection/validation: incomplete edits must
  // never prevent the operator from removing a layer from output.
  async function out(operations){
    if(!operations.length)throw Error("OFF할 레이어를 체크하세요.");
    const results=await Promise.allSettled(operations.map(operation=>Promise.resolve().then(()=>operation.hide())));
    return {off:operations.filter((_,i)=>results[i].status==="fulfilled").map(operation=>operation.name),failed:operations.filter((_,i)=>results[i].status==="rejected").map(operation=>operation.name)};
  }
  const api={take,out};if(typeof module!=="undefined"&&module.exports)module.exports=api;else root.SWStudioBatch=api;
})(typeof window!=="undefined"?window:globalThis);
