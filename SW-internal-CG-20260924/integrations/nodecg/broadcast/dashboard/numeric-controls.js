"use strict";
// Keep the existing field and its event handlers as the source of truth.
(() => {
  function install() {
    if (document.getElementById("sw-numeric-style")) return;
    const style = document.createElement("style");
    style.id = "sw-numeric-style";
    style.textContent = `.sw-numeric{display:grid!important;grid-template-columns:minmax(40px,1fr) 80px;gap:8px;align-items:center;min-width:0;width:100%}.sw-numeric input{min-width:0;width:100%!important;margin:0!important}.sw-numeric input[type=range]{padding:0!important;border:0!important;accent-color:#1769e0}.sw-numeric input:not([type=range]){padding:6px!important;font-variant-numeric:tabular-nums}.sw-numeric input[aria-invalid=true]{outline:2px solid #c5221f}.range-row:has(>.sw-numeric){grid-template-columns:minmax(0,1fr)!important}.slider-row:has(>.sw-numeric){grid-template-columns:72px minmax(0,1fr)!important}.range-row:has(>.sw-numeric)>output,.slider-row:has(>.sw-numeric)>output{display:none}`;
    document.head.append(style);
    const fields = [...document.querySelectorAll('input[type="range"],input[type="number"]')];
    const textSpecs = [
      ['#temp', -50, 50, .1], ['#reporting', 0, 100, .1],
      ['#totalBallots,[data-field="votes"]', 0, 100000000, 1],
      ['[data-field="percent"],[data-ticker-field$="Percent"]', 0, 100, .1]
    ];
    for (const [selector,min,max,step] of textSpecs) {
      for (const field of document.querySelectorAll(selector)) {
        field.dataset.numericMin = min; field.dataset.numericMax = max; field.dataset.numericStep = step;
        fields.push(field);
      }
    }
    for (const field of fields) pair(field);
  }
  function pair(field) {
    const isRange = field.type === "range", isText = field.type === "text";
    const min = Number(field.dataset.numericMin ?? (field.min || 0));
    const max = Number(field.dataset.numericMax ?? (field.max || 100));
    const step = Number(field.dataset.numericStep ?? (field.step || 1));
    const originalParent = field.parentElement;
    const label = field.labels?.[0]?.textContent || originalParent.querySelector("label")?.textContent || field.id || "수치";
    const peer = document.createElement("input");
    peer.type = isRange ? "number" : "range";
    peer.min = min; peer.max = max; peer.step = step;
    peer.setAttribute("aria-label", `${label.trim()} ${isRange ? "직접 입력" : "드래그 조절"}`);
    field.setAttribute("aria-label", field.getAttribute("aria-label") || label.trim());
    const wrapper = document.createElement("div"); wrapper.className = "sw-numeric";
    field.before(wrapper); wrapper.append(...(isRange ? [field,peer] : [peer,field]));
    const descriptor = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, "value");
    const parse = raw => {
      const clean = String(raw).trim().replace(/,/g, "").replace(/(?:℃|°C|%)$/i, "").trim();
      return /^[-+]?(?:\d+\.?\d*|\.\d+)$/.test(clean) ? Number(clean) : NaN;
    };
    let last = field.value, forwarding = false;
    const valid = raw => Number.isFinite(parse(raw)) && parse(raw) >= min && parse(raw) <= max;
    const normalized = raw => {
      const n = parse(raw);
      const clamped = Math.min(max,Math.max(min,Number.isFinite(n) ? n : parse(last)));
      return Number((min + Math.round((clamped-min)/step)*step).toFixed(6));
    };
    function sync() {
      if (document.activeElement === peer && !forwarding && isRange) return;
      if (Number.isFinite(parse(field.value))) peer.value = String(parse(field.value));
      if (valid(field.value)) last = field.value;
    }
    // Replicant callbacks assign .value directly; keep the companion control in sync too.
    Object.defineProperty(field,"value",{configurable:true,get(){return descriptor.get.call(this);},set(value){descriptor.set.call(this,value);sync();}});
    peer.addEventListener("input", () => {
      if (!valid(peer.value)) { peer.setAttribute("aria-invalid","true"); return; }
      peer.removeAttribute("aria-invalid"); forwarding = true;
      const suffix = isText ? (field.value.match(/(?:℃|°C|%)\s*$/i)?.[0] || "") : "";
      field.value = String(normalized(peer.value)) + suffix;
      field.dispatchEvent(new Event("input",{bubbles:true})); forwarding = false;
    });
    function commitPeer() {
      if (isRange) {
        peer.value = Number.isFinite(parse(peer.value)) ? normalized(peer.value) : field.value;
        peer.removeAttribute("aria-invalid");
        peer.dispatchEvent(new Event("input",{bubbles:true}));
      }
      field.dispatchEvent(new Event("change",{bubbles:true}));
      sync();
    }
    peer.addEventListener("change", commitPeer);
    peer.addEventListener("blur", () => { if (!valid(peer.value)) commitPeer(); });
    field.addEventListener("input", event => {
      if (!valid(field.value)) {
        field.setAttribute("aria-invalid","true"); event.stopImmediatePropagation(); return;
      }
      field.removeAttribute("aria-invalid"); sync();
    },true);
    function commitField() {
      if (!valid(field.value)) {
        const suffix = isText ? (last.match(/(?:℃|°C|%)\s*$/i)?.[0] || "") : "";
        const corrected = Number.isFinite(parse(field.value)) ? normalized(field.value) : parse(last);
        field.value = Number.isFinite(corrected) ? String(corrected) + suffix : String(min);
        field.removeAttribute("aria-invalid");
        field.dispatchEvent(new Event("input",{bubbles:true}));
      }
      sync();
    }
    field.addEventListener("change", commitField,true);
    field.addEventListener("blur", () => { if (!valid(field.value)) commitField(); },true);
    sync();
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded",install,{once:true});
  else install();
})();
