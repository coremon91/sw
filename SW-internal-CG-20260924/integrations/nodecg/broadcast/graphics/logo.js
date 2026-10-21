"use strict";

const scene = document.getElementById("scene");
const logoElement = document.getElementById("logo");
const previewMode =
  new URLSearchParams(window.location.search).get("preview") === "1";
const logo = nodecg.Replicant(previewMode ? "logoPreview" : "logo");
let currentValue;
let dragState;
let updateTimer;

function number(value, fallback, minimum, maximum) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.min(maximum, Math.max(minimum, parsed));
}

logo.on("change", (value) => {
  if (!value) return;
  value = SWBroadcastLayout.logo(value);
  currentValue = value;
  const source = value.image || "/bundles/my-cg/graphics/images/logo.png";
  if (logoElement.getAttribute("src") !== source) logoElement.src = source;
  logoElement.style.left = `${number(value.x, 1660, -1920, 1920)}px`;
  logoElement.style.top = `${number(value.y, 54, -1080, 1080)}px`;
  logoElement.style.width = `${number(value.width, 180, 20, 1920)}px`;
  logoElement.style.height = `${number(
    value.height,
    number(value.width, 180, 20, 1920),
    20,
    1080
  )}px`;
  logoElement.style.opacity = String(
    number(value.opacity, 100, 0, 100) / 100
  );
  scene.classList.toggle("visible", previewMode || Boolean(value.visible));
});

function clampPosition(x, y) {
  const width = number(currentValue?.width, 180, 20, 1920);
  const height = number(currentValue?.height, width, 20, 1080);
  return {
    x: Math.round(Math.min(1920 - width, Math.max(0, x))),
    y: Math.round(Math.min(1080 - height, Math.max(0, y)))
  };
}

function sendPosition(position, immediate = false) {
  clearTimeout(updateTimer);
  const send = () => {
    nodecg.sendMessage("logo:update", { ...currentValue, ...position, layoutVersion: SWBroadcastLayout.version }).catch((error) => {
      console.error("Logo preview position update failed:", error);
    });
  };
  if (immediate) send();
  else updateTimer = setTimeout(send, 35);
}

if (previewMode) {
  logoElement.classList.add("draggable");

  logoElement.addEventListener("pointerdown", (event) => {
    if (!currentValue) return;
    event.preventDefault();
    logoElement.setPointerCapture(event.pointerId);
    logoElement.classList.add("dragging");
    dragState = {
      pointerId: event.pointerId,
      clientX: event.clientX,
      clientY: event.clientY,
      x: number(currentValue.x, 1660, 0, 1920),
      y: number(currentValue.y, 54, 0, 1080)
    };
  });

  logoElement.addEventListener("pointermove", (event) => {
    if (!dragState || dragState.pointerId !== event.pointerId) return;
    const position = clampPosition(
      dragState.x + event.clientX - dragState.clientX,
      dragState.y + event.clientY - dragState.clientY
    );
    logoElement.style.left = `${position.x}px`;
    logoElement.style.top = `${position.y}px`;
    sendPosition(position);
  });

  function finishDrag(event) {
    if (!dragState || dragState.pointerId !== event.pointerId) return;
    const position = clampPosition(
      dragState.x + event.clientX - dragState.clientX,
      dragState.y + event.clientY - dragState.clientY
    );
    dragState = null;
    logoElement.classList.remove("dragging");
    sendPosition(position, true);
  }

  logoElement.addEventListener("pointerup", finishDrag);
  logoElement.addEventListener("pointercancel", finishDrag);
}
