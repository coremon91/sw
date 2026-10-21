"use strict";
(() => {
  const viewport = document.getElementById("previewViewport");
  const previewMode = document.getElementById("previewMode");
  const imageUrl = "layout-reference.png";
  const image = document.createElement("img");
  image.id = "layoutReference";
  image.alt = "";
  image.src = imageUrl;
  image.hidden = true;
  viewport.prepend(image);

  const toolbar = document.createElement("div");
  toolbar.className = "reference-toolbar";
  const button = document.createElement("button");
  button.id = "toggleReference";
  button.type = "button";
  button.setAttribute("aria-label", "첨부 위치 가이드 프리뷰 배경");
  const thumbnail = document.createElement("img");
  thumbnail.src = imageUrl;
  thumbnail.alt = "첨부 위치 가이드";
  const label = document.createElement("span");
  button.append(thumbnail, label);
  const hint = document.createElement("span");
  hint.className = "reference-hint";
  hint.textContent = "그림 클릭으로 배경 ON/OFF · 편집 PVW 전용";
  toolbar.append(button, hint);
  document.querySelector(".preview-heading").after(toolbar);

  const storageKey = "sw-cg-layout-reference-v1";
  let enabled = false;
  try { enabled = localStorage.getItem(storageKey) === "on"; } catch {}
  function render() {
    const editing = previewMode.getAttribute("aria-pressed") === "true";
    image.hidden = !enabled || !editing;
    button.disabled = !editing;
    button.setAttribute("aria-pressed", String(enabled));
    label.textContent = `위치 가이드 ${enabled ? "ON" : "OFF"}`;
  }
  button.onclick = () => {
    enabled = !enabled;
    try { localStorage.setItem(storageKey, enabled ? "on" : "off"); } catch {}
    render();
  };
  new MutationObserver(render).observe(previewMode, {attributes:true,attributeFilter:["aria-pressed"]});
  render();
})();
